#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import xxhash
import os.path
import argparse
import functools

from dwarfvars import DwarfVars

from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection, NoteSection
from elftools.elf.constants import P_FLAGS

PAGE_SIZE = 4096

def strcmp(a, b):
	if a < b:
		return -1
	elif a > b:
		return 1
	else:
		return 0

def compare(a, b):
	if a['category'] != b['category']:
		return strcmp(a['category'], b['category'])
	elif a['value'] == b['value']:
		return strcmp(a['name'], b['name'])
	else:
		return a['value'] - b['value']

def sortuniq(symlist):
	l = None
	for s in sorted(symlist, key=functools.cmp_to_key(compare)):
		if l and l['value'] == s['value'] and l['name'] == s['name']:
			if d['size'] != s['size']:
				print(f"Size mismatch for duplicate {l['name']}: {l['size']} vs {s['size']}", file=sys.stderr)
		else:
			l = s
			yield s

if __name__ == '__main__':
	# Arguments
	parser = argparse.ArgumentParser(prog='PROG')
	parser.add_argument('-a', '--aliases', action='store_true', help='Include aliases (typedefs)')
	parser.add_argument('-v', '--verbose', action='store_true', help='Verbose output')
	parser.add_argument('-d', '--dwarf', action='store_true', help='use DWARF')
	parser.add_argument('-r', '--root', help='Path prefix for debug symbol files', default='')
	parser.add_argument('-w', '--writable', action='store_true', help='Ignore non-writable sections')
	parser.add_argument('-i', '--identical', action='store_true', help='Check if hashes of input files are identical')
	parser.add_argument('-n', '--names', action='store_true', help='Include names (complex types / members)')
	parser.add_argument('file', type=argparse.FileType('rb'), help="ELF file with debug information", nargs='+')
	args = parser.parse_args()

	files = {}
	buildids = {}
	dbgfiles = {}
	for f in args.file:
		symbols = []
		elffile = ELFFile(f)
		filepath = os.path.realpath(f.name)
		seginfo = []
		sec2seg = {}
		categories = set()
		for segment in elffile.iter_segments():
			if segment['p_type'] in [ 'PT_LOAD', 'PT_TLS' ]:
				cat = ''
				if segment['p_flags'] & P_FLAGS.PF_R != 0:
					cat += 'R'
				if segment['p_flags'] & P_FLAGS.PF_W != 0:
					cat += 'W'
				if segment['p_flags'] & P_FLAGS.PF_X != 0:
					cat += 'X'
				if segment['p_type'] == 'PT_TLS':
					cat = 'TLS'
				categories.add(cat)

				seginfo.append({
					'category': cat,
					'value': segment['p_vaddr'],
					'size': segment['p_memsz']
				})
				segidx = len(seginfo) - 1

				for s in range(elffile.num_sections()):
					if segment.section_in_segment(elffile.get_section(s)):
						sec2seg[s] = segidx

		for section in elffile.iter_sections():
			if isinstance(section, SymbolTableSection):
				for sym in section.iter_symbols():
					if sym['st_shndx'] != 'SHN_UNDEF' and sym['st_info']['type'] in [ 'STT_OBJECT', 'STT_TLS' ] and sym['st_size'] > 0:
						extern = True if sym['st_info']['bind'] == 'STB_GLOBAL' else False
						segment = seginfo[sec2seg[sym['st_shndx']]]
						assert(sym['st_value'] - segment['value'] + sym['st_size'] <= segment['size'])
						symbols.append({
							'name': sym.name,
							'value': sym['st_value'] - segment['value'],
							'size': sym['st_size'],
							'align': sym['st_value'] % PAGE_SIZE,
							'category': 'TLS' if sym['st_info']['type'] == 'STT_TLS' else segment['category'],
							'external': True if sym['st_info']['bind'] == 'STB_GLOBAL' else False
						})
			elif isinstance(section, NoteSection):
				for note in section.iter_notes():
					if note['n_type'] == 'NT_GNU_BUILD_ID':
						buildids[f.name] = note['n_desc']

		seginfo.sort(key=functools.cmp_to_key(compare))

		if args.dwarf:
			if elffile.has_dwarf_info() and next(elffile.get_dwarf_info().iter_CUs(), False):
				dbgfiles[f.name] = filepath
			else:
				dbgsyms = []
				if f.name in buildids:
					dbgsyms.append(args.root + '/usr/lib/debug/.build-id/' + buildids[f.name][:2] + '/' + buildids[f.name][2:] + '.debug')
				dbgsyms.append(args.root + filepath + '.debug')
				dbgsyms.append(args.root + os.path.dirname(filepath) + '/.debug/' + os.path.basename(filepath) + '.debug')
				dbgsyms.append(args.root + '/usr/lib/debug' + filepath + '.debug')
				for dbgsym in dbgsyms:
					if os.path.exists(dbgsym):
						dbgfiles[f.name] = dbgsym
						break

			if f.name in dbgfiles:
				dwarf = DwarfVars(dbgfiles[f.name], aliases = args.aliases, names = args.names)

				# Prepare formatr of dwarf variables
				dwarfsyms = dwarf.get_vars(tls = False)
				for dvar in dwarfsyms:
					for seg in seginfo:
						if dvar['value'] >= seg['value'] and dvar['value'] + dvar['size'] <= seg['value'] + seg['size']:
							dvar['align'] = dvar['value']  % PAGE_SIZE;
							dvar['value'] = dvar['value'] - seg['value']
							dvar['category'] = seg['category']

				for dvar in dwarf.get_vars(tls = True):
					dvar['align'] = dvar['value']  % PAGE_SIZE;
					dvar['category'] = 'TLS'
					dwarfsyms.append(dvar)

				variables = []
				si = sortuniq(symbols)
				di = sortuniq(dwarfsyms)

				s = next(si, None)
				d = next(di, None)
				while s or d:
					if not s:
						variables.append(d)
						d = next(di, None)
					elif not d:
						variables.append(s)
						s = next(si, None)
					elif d['value'] == s['value'] and s['name'].startswith(d['name']):
						if d['size'] != s['size']:
							print(f"Size mismatch for {s['name']}: {d['size']} vs {s['size']}", file=sys.stderr)
						if d['external'] != s['external']:
							print(f"External mismatch for {s['name']}", file=sys.stderr)
						v = s
						for key in [ 'type', 'hash', 'source' ]:
							v[key] = d[key]
						variables.append(v)
						s = next(si, None)
						d = next(di, None)
					else:
						c = compare(d, s)
						assert(c != 0)
						if c < 0:
							variables.append(d)
							d = next(di, None)
						else:
							print(f"No DWARF def of {s['name']} found", file=sys.stderr)
							variables.append(s)
							s = next(si, None)
		else:
			variables = list(sortuniq(symbols))


		files[f.name] = {}
		for cat in sorted(categories):
			if args.writable and not 'W' in cat and cat != 'TLS':
				continue

			hash = xxhash.xxh64()

			segstr = []
			for seg in seginfo:
				if seg['category'] == cat:
					hash.update('+' + str(seg['value'] % PAGE_SIZE) + ':' + str(seg['size']))
					segstr.append(hex(seg['value']) + ':' + str(seg['size']))

			vars = filter(lambda x: x['category'] == cat, variables)
			varstr = []
			for var in vars:
				varstr.append(str(var))
				if args.names:
					hash.update(var['name'])
				if 'hash' in var:
					hash.update('#' + var['hash'])
				hash.update('@' + str(var['value']) + '/' + str(var['align']) + ':' + str(var['size']))

			if len(varstr) > 0:
				files[f.name][cat] = (hash.hexdigest(), '(' + ', '.join(segstr) + ")\n  [\n    " + "\n    ".join(varstr) + "\n  ]" if args.verbose else '')

	identical = True
	for f in args.file:
		if f.name in files:
			names = [ f.name ]
			data = files.pop(f.name)
			others = list(files.keys())
			for other in others:
				if data == files[other]:
					names.append(other)
					del files[other]
			if len(files) > 0:
				identical = False
			for name in sorted(names):
				str = '# ' + os.path.realpath(name).lstrip(args.root)
				if name in buildids:
					str = str + ' [' + buildids[name] + ']'
				if name in dbgfiles:
					str = str + ' (' + dbgfiles[name].lstrip(args.root) + ')'
				print(str)
			for cat in sorted(data):
				print(data[cat][0], cat, data[cat][1])

	if args.identical:
		sys.exit(0 if identical else 1)

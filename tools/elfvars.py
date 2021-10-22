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

if __name__ == '__main__':
	# Arguments
	parser = argparse.ArgumentParser(prog='PROG')
	parser.add_argument('-a', '--aliases', action='store_true', help='Include aliases (typedefs)')
	parser.add_argument('-v', '--verbose', action='store_true', help='Verbose output')
	parser.add_argument('-d', '--dwarf', action='store_true', help='use DWARF')
	parser.add_argument('-D', '--debugroot', help='Path prefix for debug symbol files', default='')
	parser.add_argument('-i', '--identical', action='store_true', help='Check if hashes of input files are identical')
	parser.add_argument('-n', '--names', action='store_true', help='Include names (complex types / members)')
	parser.add_argument('file', type=argparse.FileType('rb'), help="ELF file with debug information", nargs='+')
	args = parser.parse_args()

	files = {}
	dbgfiles = {}
	for f in args.file:
		variables = []
		elffile = ELFFile(f)
		filepath = os.path.realpath(f.name)
		buildid = None
		shndx_cat = {}
		categories = set()
		for segment in elffile.iter_segments():
			if segment['p_type'] == 'PT_LOAD':
				for s in range(elffile.num_sections()):
					if segment.section_in_segment(elffile.get_section(s)):
						shndx_cat[s] = ''
						if segment['p_flags'] & P_FLAGS.PF_R != 0:
							shndx_cat[s] += 'R'
						if segment['p_flags'] & P_FLAGS.PF_W != 0:
							shndx_cat[s] += 'W'
						if segment['p_flags'] & P_FLAGS.PF_X != 0:
							shndx_cat[s] += 'X'
						categories.add(shndx_cat[s])
			elif segment['p_type'] == 'PT_TLS':
				categories.add('TLS')

		for section in elffile.iter_sections():
			if isinstance(section, SymbolTableSection):
				for sym in section.iter_symbols():
					if sym['st_shndx'] != 'SHN_UNDEF' and sym['st_info']['type'] in [ 'STT_OBJECT', 'STT_TLS' ] and sym['st_size'] > 0:
						extern = True if sym['st_info']['bind'] == 'STB_GLOBAL' else False
						variables.append({
							'name': sym.name,
							'value': sym['st_value'],
							'size': sym['st_size'],
							'category': 'TLS' if sym['st_info']['type'] == 'STT_TLS' else shndx_cat[sym['st_shndx']],
							'external': True if sym['st_info']['bind'] == 'STB_GLOBAL' else False
						})
			elif isinstance(section, NoteSection):
				for note in section.iter_notes():
					if note['n_type'] == 'NT_GNU_BUILD_ID':
						buildid = note['n_desc']

		if args.dwarf:
			if elffile.has_dwarf_info() and next(elffile.get_dwarf_info().iter_CUs(), False):
				dbgfiles[f.name] = filepath
			else:
				dbgsyms = []
				if buildid:
					dbgsyms.append(args.debugroot + '/usr/lib/debug/.build-id/' + buildid[:2] + '/' + buildid[2:] + '.debug')
				dbgsyms.append(args.debugroot + filepath + '.debug')
				dbgsyms.append(args.debugroot + os.path.dirname(filepath) + '/.debug/' + os.path.basename(filepath) + '.debug')
				dbgsyms.append(args.debugroot + '/usr/lib/debug' + filepath + '.debug')
				for dbgsym in dbgsyms:
					if os.path.exists(dbgsym):
						dbgfiles[f.name] = dbgsym
						break

			if f.name in dbgfiles:
				dwarf = DwarfVars(dbgfiles[f.name], aliases = args.aliases, names = args.names)
				dwarfvars = dwarf.get_vars(tls = False)
				dwarfvars_tls = dwarf.get_vars(tls = True)

				# O(n^2), just a prototype
				for var in variables:
					found = False
					dvars = dwarfvars_tls if var['category'] == 'TLS' else dwarfvars
					for dvar in dvars:
						if dvar['value'] == var['value'] and dvar['name'].startswith(var['name']):
							if dvar['size'] != var['size']:
								print(f"Size mismatch for {var['name']}: {dvar['size']} vs {var['size']}", file=sys.stderr)
							if dvar['external'] != var['external']:
								print(f"External mismatch for {var['name']}", file=sys.stderr)
							for key in [ 'type', 'hash', 'source' ]:
								var[key] = dvar[key]
							found = True
							break
					if not found:
						print(f"No DWARF def of {var['name']} found", file=sys.stderr)

		variables.sort(key=functools.cmp_to_key(compare))

		files[f.name] = {}
		for cat in sorted(categories):
			vars = filter(lambda x: x['category'] == cat, variables)
			hash = xxhash.xxh64()
			output = '[ '
			for var in vars:
				output += str(var) + ', '
				if args.names:
					hash.update(var['name'])
				if 'hash' in var:
					hash.update('#' + var['hash'])
				hash.update('@' + str(var['value']) + ':' + str(var['size']))
			output += ']'
			if output != '[ ]':
				files[f.name][cat] = (hash.hexdigest(), "\n  " + output.replace("{'name': ", "\n    {'name': ")[:-3] + "\n  ]" if args.verbose else '')

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
				print('#', os.path.realpath(name), '(' + dbgfiles[f.name] + ')' if f.name in dbgfiles else '')
			for cat in sorted(data):
				print(data[cat][0], cat, data[cat][1])

	if args.identical:
		sys.exit(0 if identical else 1)

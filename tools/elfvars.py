#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import io
import sys
import errno
import xxhash
import os.path
import argparse
import functools

from dwarfvars import get_debugbin, DwarfVars

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

def compare_symbols(a, b):
	if a['category'] != b['category']:
		return strcmp(a['category'], b['category'])
	elif a['value'] == b['value']:
		return strcmp(a['name'], b['name'])
	else:
		return a['value'] - b['value']

def sortuniq(symlist):
	l = None
	for s in sorted(symlist, key=functools.cmp_to_key(compare_symbols)):
		if l and l['value'] == s['value'] and l['name'] == s['name']:
			if d['size'] != s['size']:
				print(f"Size mismatch for duplicate {l['name']}: {l['size']} vs {s['size']}", file=sys.stderr)
		else:
			l = s
			yield s

class ElfVar:
	def __init__(self, file, root='', dbgsym = True, dbgsym_extern = True, aliases = True, names = True):
		# Find ELF file
		if isinstance(file, str):
			if os.path.exists(file):
				self.path = os.path.realpath(file)
			elif os.path.exists(root + '/' + file):
				self.path = os.path.realpath(root + '/' + file)
			else:
				raise FileNotFoundError(errno.ENOENT, os.strerror(errno.ENOENT), file)
			self.elf = ELFFile(open(self.path, "rb"))
		elif isinstance(file, io.BufferedReader):
			self.path = os.path.realpath(file.name)
			self.elf = ELFFile(file)
		else:
			raise TypeError(f"File has invalid type {type(file)}")

		# Parse segments
		self.segments = []
		self.categories = set()
		self.sec2seg = {}
		self.relro = None
		self.dwarf = None
		for segment in self.elf.iter_segments():
			if segment['p_type'] in [ 'PT_LOAD', 'PT_TLS', 'PT_GNU_RELRO' ]:
				cat = ''
				if segment['p_flags'] & P_FLAGS.PF_R != 0:
					cat += 'R'
				if segment['p_flags'] & P_FLAGS.PF_W != 0:
					cat += 'W'
				if segment['p_flags'] & P_FLAGS.PF_X != 0:
					cat += 'X'
				if segment['p_type'] == 'PT_TLS':
					cat = 'TLS'
				self.categories.add(cat)

				data = {
					'category': cat,
					'value': segment['p_vaddr'],
					'size': segment['p_memsz']
				}

				if segment['p_type'] == 'PT_GNU_RELRO':
					self.relro = data
				else:
					self.segments.append(data)
					segidx = len(self.segments) - 1

					for s in range(self.elf.num_sections()):
						if segment.section_in_segment(self.elf.get_section(s)):
							self.sec2seg[s] = segidx

		# Get build ID
		self.buildid = None
		for section in self.elf.iter_sections():
			if isinstance(section, NoteSection):
				for note in section.iter_notes():
					if note['n_type'] == 'NT_GNU_BUILD_ID':
						self.buildid = note['n_desc']

		# Get debug symbols
		if self.elf.has_dwarf_info() and next(self.elf.get_dwarf_info().iter_CUs(), False):
			dbgsym = self.path
		elif dbgsym_extern:
			dbgsym = get_debugbin(self.path, root, self.buildid)
		else:
			dbgsym = None

		if dbgsym:
			self.dwarf = DwarfVars(dbgsym, aliases = aliases, names = names)


	def symbols(self):
		symbols = []
		for section in self.elf.iter_sections():
			if isinstance(section, SymbolTableSection):
				for sym in section.iter_symbols():
					if sym['st_shndx'] != 'SHN_UNDEF' and sym['st_info']['type'] in [ 'STT_OBJECT', 'STT_TLS' ] and sym['st_size'] > 0:
						extern = True if sym['st_info']['bind'] == 'STB_GLOBAL' else False
						segment = self.segments[self.sec2seg[sym['st_shndx']]]
						if self.relro and 'W' in segment['category'] and sym['st_value'] >= self.relro['value'] and sym['st_value'] + sym['st_size'] < self.relro['value'] + self.relro['size']:
							segment = self.relro
						#assert(sym['st_value'] - segment['value'] + sym['st_size'] <= segment['size'])
						symbols.append({
							'name': sym.name,
							'value': sym['st_value'] - segment['value'],
							'size': sym['st_size'],
							'align': sym['st_value'] % PAGE_SIZE,
							'category': 'TLS' if sym['st_info']['type'] == 'STT_TLS' else segment['category'],
							'external': True if sym['st_info']['bind'] == 'STB_GLOBAL' else False
						})
		return symbols

	def symbols_debug(self):
		# Prepare format of dwarf variables
		dwarfsyms = self.dwarf.get_vars(tls = False)
		for dvar in dwarfsyms:
			for seg in self.segments:
				if dvar['value'] >= seg['value'] and dvar['value'] + dvar['size'] <= seg['value'] + seg['size']:
					if self.relro and 'W' in seg['category'] and dvar['value'] >= self.relro['value'] and dvar['value'] + dvar['size'] < self.relro['value'] + self.relro['size']:
						seg = self.relro
					dvar['align'] = dvar['value'] % PAGE_SIZE;
					dvar['value'] = dvar['value'] - seg['value']
					dvar['category'] = seg['category']
					break
			else:
				raise RuntimeError("No segment found for address {}".format(hex(dwarf['value'])))

		for dvar in self.dwarf.get_vars(tls = True):
			dvar['align'] = dvar['value']  % PAGE_SIZE;
			dvar['category'] = 'TLS'
			dwarfsyms.append(dvar)

		return dwarfsyms


if __name__ == '__main__':
	# Arguments
	parser = argparse.ArgumentParser(prog='PROG')
	parser.add_argument('-a', '--aliases', action='store_true', help='Include aliases (typedefs)')
	parser.add_argument('-v', '--verbose', action='store_true', help='Verbose output')
	parser.add_argument('-d', '--dbgsym', action='store_true', help='use debug symbols')
	parser.add_argument('-D', '--dbgsym_extern', action='store_true', help='use external debug symbols (implies -d)')
	parser.add_argument('-r', '--root', help='Path prefix for debug symbol files', default='')
	parser.add_argument('-t', '--datatypes', action='store_true', help='Hash datatypes (requires debug symbols)')
	parser.add_argument('-w', '--writable', action='store_true', help='Ignore non-writable sections')
	parser.add_argument('-i', '--identical', action='store_true', help='Check if hashes of input files are identical')
	parser.add_argument('-n', '--names', action='store_true', help='Include names (complex types / members)')
	parser.add_argument('file', type=argparse.FileType('rb'), help="ELF file with debug information", nargs='+')
	args = parser.parse_args()

	files = {}
	datatypes = {}
	buildids = {}
	dbgfiles = {}
	for f in args.file:
		elf = ElfVar(f, args.root, args.dbgsym, args.dbgsym_extern, args.aliases, args.names)

		symbols = elf.symbols()

		variables = []

		if args.dbgsym and elf.dwarf:
			si = sortuniq(symbols)
			di = sortuniq(elf.symbols_debug())

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
						raise RuntimeError(f"Size mismatch for {s['name']}: {d['size']} vs {s['size']}")
					if d['category'] != s['category']:
						raise RuntimeError(f"category mismatch for {s['name']}: {d['category']} vs {s['category']}")
					if d['external'] != s['external']:
						raise RuntimeError(f"External mismatch for {s['name']}")
					v = s
					for key in [ 'type', 'hash', 'source' ]:
						v[key] = d[key]
					variables.append(v)
					s = next(si, None)
					d = next(di, None)
				else:
					c = compare_symbols(d, s)
					assert(c != 0)
					if c < 0:
						variables.append(d)
						d = next(di, None)
					else:
						if not s['external']:
							RuntimeError(f"No DWARF def of {s['name']} found")
						variables.append(s)
						s = next(si, None)
		else:
			variables = list(sortuniq(symbols))


		files[f.name] = {}
		for cat in sorted(elf.categories):
			if args.writable and not 'W' in cat and cat != 'TLS':
				continue

			hash = xxhash.xxh64()

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
				files[f.name][cat] = (hash.hexdigest(), '\n  [\n    ' + "\n    ".join(varstr) + "\n  ]" if args.verbose else '')

		if args.datatypes and elf.dwarf:
			datatypes = []
			for id, size, hash in elf.dwarf.iter_types():
				if size > 0:
					datatypes.append({
						"type": id,
						"size": size,
						"hash": hash
					})

			if len(datatypes) > 0:
				datatypes.sort(key = lambda x: x['type'])
				hash = xxhash.xxh64()
				datastr = []
				for t in datatypes:
					datastr.append(str(t))
					hash.update(t['hash'])

				files[f.name]["TYPE"] = (hash.hexdigest(), '\n  [\n    ' + "\n    ".join(datastr) + "\n  ]" if args.verbose else '')

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

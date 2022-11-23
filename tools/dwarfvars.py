#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import errno
import regex
import xxhash
import argparse
import subprocess
from libdebuginfod import DebugInfoD

# TODO: Use pyelftools instead

def get_debugbin(filepath, root = '', buildid = None, debuginfod = True):
	dbgsyms = []
	ids = []
	if buildid:
		dbgsyms.append(root + '/usr/lib/debug/.build-id/' + buildid[:2] + '/' + buildid[2:] + '.debug')
		ids.append(buildid)
	else:
		# Debug symbols via Build ID
		buildids = regex.compile(r'^\s*Build-ID: ([0-9a-f]+)$')
		readelf = subprocess.Popen(['readelf', '-n', root + filepath], stdout=subprocess.PIPE)
		while line := readelf.stdout.readline().decode('utf-8'):
			if buildid := buildids.match(line):
				dbgsyms.append(root + '/usr/lib/debug/.build-id/' + buildid.group(1)[:2] + '/' + buildid.group(1)[2:] + '.debug')
				ids.append( buildid.group(1))

	dbgsyms.append(root + filepath + '.debug')
	dbgsyms.append(root + os.path.dirname(filepath) + '/.debug/' + os.path.basename(filepath) + '.debug')
	dbgsyms.append(root + '/usr/lib/debug' + filepath + '.debug')

	for dbgsym in dbgsyms:
		if os.path.exists(dbgsym):
			return dbgsym

	if debuginfod:
		try:
			session = DebugInfoD()
			session.begin()
			for id in ids:
				fd, path = session.find_debuginfo(id)
				if path:
					session.end()
					os.close(fd)
					return path
			session.end()
		except:
			return None

	return None

class DwarfVars:
	def __init__(self, file, aliases = True, names = True, external_dbgsym = True, root = ''):
		self.DIEs = []
		self.aliases = aliases
		self.names = names
		self.file = os.path.realpath(file)
		self.root = root
		if not os.path.exists(self.file):
			self.file = os.path.realpath(root + file)
		if len(root) > 0 and self.filepath.startswith(root):
			self.filepath = self.filepath[len(root):]
		self.dbgsym = None

		if not os.path.exists(root + self.file):
			raise FileNotFoundError(errno.ENOENT, os.strerror(errno.ENOENT), file)
		elif self.parse(file):
			self.dbgsym = file
		elif external_dbgsym:
			self.dbgsym = get_debugbin(file, root)

		# Not found:
		if not self.dbgsym:
			raise FileNotFoundError(errno.ENOENT, "No Debug Information available", file)


	def parse(self, file):
		entries = regex.compile(r'^<(\d+)><(0x[0-9a-f]+)(?:\+0x[0-9a-f]+)?><([^>]+)>(.*)$')
		attribs = regex.compile(r' ([^<>]+)(<((?>[^<>]+|(?2)))>)')
		hexvals = regex.compile(r'^[<]?(0x[0-9a-f]+|[0-9]+)(:? \(-[0-9]+\))?[>]?$')
		level = 0
		last = 0
		unit = 0
		dwarfdump = subprocess.Popen(['dwarfdump', '-i', '-e', '-d', file], stdout=subprocess.PIPE)
		while line := dwarfdump.stdout.readline().decode('utf-8'):
			if entry := entries.match(line):
				DIE = {}
				ID = int(entry.group(2), 0)

				DIE['tag'] = entry.group(3)
				DIE['children'] = []

				if entry.group(3) == 'compile_unit':
					assert(entry.group(1) == '0')
					DIE['parent'] = ID
					self.DIEs.append({})
					unit = len(self.DIEs) - 1
				else:
					# Nesting level
					l = int(entry.group(1))
					assert(l > 0)
					if l > level:
						assert(l == level + 1)
						parent = last
						level = l
					else:
						parent = self.DIEs[unit][last]['parent']
						while l < level:
							parent = self.DIEs[unit][parent]['parent']
							level = level - 1
						assert(level == l)

					# Set parent
					DIE['parent'] = parent
					# In parent add child
					self.DIEs[unit][parent]['children'].append(ID)
				last = ID

				# Additional attributes
				for attrib in attribs.finditer(entry.group(4)):
					value = attrib.group(3)
					if hexval := hexvals.match(value):
						try:
							value = int(hexval.group(1), 0)
						except ValueError:
							value = int(hexval.group(1))
					DIE[attrib.group(1)] = value

				# Reduce entries by combining declaration source
				if 'decl_file' in DIE and 'decl_line' in DIE:
					DIE['decl'] = DIE['decl_file'].split(" ", 1)[1] + ':' + str(DIE['decl_line'])
					del DIE['decl_file']
					del DIE['decl_line']
					if 'decl_column' in DIE:
						DIE['decl'] += ':' + str(DIE['decl_column'])
						del DIE['decl_column']

				DIE['unit'] = unit
				self.DIEs[unit][ID] = DIE
		return len(self.DIEs) > 0

	def get_type(self, DIE, resolve_members = True):
		if not resolve_members and 'children' in DIE:
			# We are not able to resolve the members yet
			# so we use special keys to cache the results
			key_id = 'identifier_flat'
			key_hash = 'hash_flat'
		else:
			key_id = 'identifier'
			key_hash = 'hash'

		if not key_id in DIE:
			hash = xxhash.xxh64()
			id = ''
			size = 0
			factor = 1
			use_type_hash = False
			include_members = False

			# Hash type
			hash.update('%' + DIE['tag'])

			if DIE['tag'] == 'structure_type':
				id = 'struct'
				include_members = True
			elif DIE['tag'] == 'class_type':
				id = 'class'
				include_members = True
			elif DIE['tag'] == 'union_type':
				id = 'union'
				include_members = True
			elif DIE['tag'] == 'enumeration_type':
				id = 'enum'
				include_members = True
			elif DIE['tag'] == 'const_type':
				# Ignore const if not alias
				if self.aliases or not 'type' in DIE:
					id = 'const'
				else:
					use_type_hash = True
			elif DIE['tag'] == 'typedef':
				# ignore typedef if not alias
				if self.aliases or not 'type' in DIE:
					identifier = 'typedef'
				else:
					use_type_hash = True
			elif DIE['tag'] == 'pointer_type':
				resolve_members = False

			if 'name' in DIE and self.names:
				hash.update('.' + DIE['name'])
				if len(id) > 0:
					id += ' '
				id += DIE['name']
			elif 'linkage_name' in DIE and self.names:
				hash.update('.' + DIE['linkage_name'])
				if len(id) > 0:
					id += ' '
				id += DIE['linkage_name']

			if include_members and resolve_members:
				id += ' { '
				for child in self.iter_children(DIE):
					if child['tag'] == 'member':
						child_id, child_size, child_hash = self.get_type(child, resolve_members)
						hash.update('>' + child_hash)
						id += child_id
						# Struct members contain offset (due to padding)
						if 'data_member_location' in child:
							hash.update('@' + str(child['data_member_location']))
							id += ' @ ' + str(child['data_member_location'])
						id += '; '
					elif child['tag'] == 'enumerator':
						hash.update('>' + child['name']+ '=' + str(child['const_value']))
						id += child['name'] + ' = ' + str(child['const_value']) + ', '
				id += '}'

			if 'type' in DIE:
				type_id, type_size, type_hash = self.get_type(self.DIEs[DIE['unit']][DIE['type']], resolve_members)
				id += '(' + type_id + ')' if len(id) > 0 else type_id
				size = type_size
				hash.update('#' + type_hash)

			if DIE['tag'] == 'pointer_type':
				id += '*'
			elif DIE['tag'] == 'array_type':
				for child in self.iter_children(DIE):
					if child['tag'] == 'subrange_type':
						lower = child.get('lower_bound', 0)
						upper = child.get('upper_bound', 0)
						hash.update('[' + str(lower) + ':' + str(upper) + ']')
						subrange = upper - lower + 1
						id += '[' + str(subrange) + ']'
						factor *= subrange

			if 'byte_size' in DIE:
				size = DIE['byte_size']

			if 'encoding' in DIE:
				assert factor == 1
				hash.update(DIE['encoding'])
				enc =  str(size) + " byte " + DIE['encoding']
				id += '(' + enc + ')' if len(id) > 0 else enc

			hash.update(':' + str(size) + '*' + str(factor))
			DIE[key_id] = id
			if 'total_size' in DIE:
				assert(DIE['total_size'] == factor * size)
			else:
				DIE['total_size'] = factor * size
			DIE[key_hash] = type_hash if use_type_hash else hash.hexdigest()

		return DIE[key_id], DIE['total_size'], DIE[key_hash]


	def get_def(self, DIE, resolve_members = True, skip_const = False):
		size = 0
		factor = 1
		include_members = False
		cdef = ''

		if DIE['tag'] == 'structure_type':
			cdef = 'struct'
			include_members = True
		elif DIE['tag'] == 'class_type':
			cdef = 'class'
			include_members = True
		elif DIE['tag'] == 'union_type':
			cdef = 'union'
			include_members = True
		elif DIE['tag'] == 'enumeration_type':
			cdef = 'enum'
			include_members = True
		elif DIE['tag'] == 'const_type':
			if not skip_const:
				cdef = 'const '
				skip_const = True
		elif DIE['tag'] == 'pointer_type':
			resolve_members = False

		name = None
		if 'name' in DIE:
			name = DIE['name']
		elif 'linkage_name' in DIE:
			name = DIE['linkage_name'];

		if include_members and resolve_members:
			if name:
				cdef += ' ' + name
				name = None
			cdef += ' { '
			for child in self.iter_children(DIE):
				if child['tag'] == 'member':
					child_cdef, child_size, child_factor = self.get_def(child, resolve_members, False)
					cdef += child_cdef
					if child_factor != 1:
						cdef += '[' + str(child_factor) + ']'
					cdef += '; '
					if 'data_member_location' in child:
						cdef += ' /* offset ' + str(child['data_member_location']) + ' */'
				elif child['tag'] == 'enumerator':
					cdef += child['name'] + ' = ' + str(child['const_value']) + ', '
			cdef += '}'

		if 'type' in DIE and (DIE['tag'] != 'typedef' or self.aliases):
			type_cdef, type_size, factor = self.get_def(self.DIEs[DIE['unit']][DIE['type']], resolve_members, skip_const)
			cdef += type_cdef
			size = type_size

		if DIE['tag'] == 'pointer_type':
			if not 'type' in DIE or len(cdef) == 0:
				cdef += 'void'
			cdef += '*'
			factor = 1
		elif DIE['tag'] == 'array_type':
			for child in self.iter_children(DIE):
				if child['tag'] == 'subrange_type':
					factor *= child.get('upper_bound', 0) - child.get('lower_bound', 0) + 1

		if name and self.names and (DIE['tag'] != 'typedef' or not self.aliases):
			if len(cdef) > 0:
				cdef += ' '
			cdef += name
			if factor != 1:
				cdef += '[' + str(factor) + ']'
				factor = 1

		if 'byte_size' in DIE:
			size = DIE['byte_size']

		if 'total_size' in DIE:
			assert(DIE['total_size'] == factor * size)
		else:
			DIE['total_size'] = factor * size

		return cdef, DIE['total_size'], factor

	def get_vars(self, tls = False):
		if tls:
			locaddr = regex.compile(r'^.*: const[0-9su]+ ([0-9a-f]+) GNU_push_tls_address$')
		else:
			locaddr = regex.compile(r'^.*: addr (0x[0-9a-f]+)$')
		variables = []
		for DIEs in self.DIEs:
			for ID, DIE in DIEs.items():
				if DIE['tag'] == 'variable' and 'location' in DIE and 'type' in DIE:
					if loc := locaddr.match(DIE['location']):
						addr = int(loc.group(1), 0)
						typename, size, hash = self.get_type(self.DIEs[DIE['unit']][DIE['type']])
						variables.append({
							'name': DIE['name'],
							'value': addr,
							'type': typename,
							'unit': DIE['unit'],
							'size': size,
							'external': True if 'external' in DIE and DIE['external'][:3] == 'yes' else False,
							'hash': hash,
							'source': DIE['decl'] if 'decl' in DIE else ''
						})
		return variables


	def iter_children(self,  DIE):
		if 'children' in DIE:
			for CID in DIE['children']:
				yield self.DIEs[DIE['unit']][CID]


	def iter_types(self):
		type_tags = ['structure_type', 'class_type', 'union_type', 'enumeration_type']
		for DIEs in self.DIEs:
			for ID, DIE in DIEs.items():
				if DIE['tag'] in type_tags:
					yield self.get_type(DIE)

	def iter_globals(self):
		type_tags = ['variable', 'structure_type', 'class_type', 'union_type', 'enumeration_type']
		for unit, DIEs in enumerate(self.DIEs):
			for ID, DIE in DIEs.items():
				if DIE['tag'] in type_tags and ('name' in DIE or 'linkage_name' in DIE):
					cdef, size, factor = self.get_def(DIE)
					assert(factor == 1)
					if size > 0:
						yield cdef, size, DIE

if __name__ == '__main__':
	# Arguments
	parser = argparse.ArgumentParser(prog='PROG')
	parser.add_argument('-a', '--aliases', action='store_true', help='Include aliases (typedefs)')
	parser.add_argument('-n', '--names', action='store_true', help='Include names (complex types / members)')
	subparsers = parser.add_subparsers(title='Extract', dest='extract', required=True, help='Information to extract')
	parser.add_argument('file', metavar="FILE", help="ELF file with debug information")
	parser_var = subparsers.add_parser('variables', help='All static variables')
	parser_var.add_argument('-s', '--source', action='store_true', help='Include source code reference comment')
	parser_var.add_argument('-t', '--tls', action='store_true', help='Extract TLS variables')
	parser_var.add_argument('-j', '--json', action='store_true', help='Output as JSON')
	parser_data = subparsers.add_parser('datatypes', help='All data types (struct, union, enum) from file')
	parser_data = subparsers.add_parser('globals', help='All global data types')
	args = parser.parse_args()

	if not os.path.exists(args.file):
		print(f"Input file '{args.file}' does not exist!", file=sys.stderr)
		sys.exit(1)

	dwarf = DwarfVars(args.file, aliases = args.aliases, names = args.names)
	if args.extract == 'variables':
		variables = sorted(dwarf.get_vars(args.tls), key=lambda i: i['value'])
		if args.json:
			print(variables)
		else:
			for var in variables:
				extern = 'extern ' if var['external'] else ''
				source = f" /* {var['source']} */" if args.source else ''
				print(f"{extern}{var['name']}({var['type']}) {var['size']} byte @ {var['value']:016x} # {var['hash']} {source}")
	elif args.extract == 'datatypes':
		# TODO: Sort by compile unit
		full_hash = xxhash.xxh64()
		for id, size, hash in dwarf.iter_types():
			full_hash.update(hash)
			print(f"{id} {size} bytes # {hash}")
		print(full_hash.hexdigest())
	elif args.extract == 'globals':
		for cdef, size, DIE in dwarf.iter_globals():
			print(f"{cdef};  // {size} bytes")

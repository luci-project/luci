#!/usr/bin/env python3
import os
import sys
import regex
import argparse
import subprocess

# Arguments
parser = argparse.ArgumentParser(prog='PROG')
parser.add_argument('-a', '--aliases', action='store_true', help='Include aliases (typedefs)')
parser.add_argument('-s', '--source', action='store_true', help='Include source code reference comment')
subparsers = parser.add_subparsers(title='Extract', dest='extract', required=True, help='Information to extract')
parser.add_argument('file', metavar="FILE", help="ELF file with debug information")
parser_data = subparsers.add_parser('datatypes', help='All data types (struct, union, enum) from file')
parser_data.add_argument('-n', '--names', action='store_true', help='Include variable names')
parser_var = subparsers.add_parser('variables', help='All static variables')
args = parser.parse_args()

# Regex
entries = regex.compile(r'^<(\d+)><(0x[0-9a-f]+)(?:\+0x[0-9a-f]+)?><([^>]+)>(.*)$')
attribs = regex.compile(r' ([^<>]+)(<((?>[^<>]+|(?2)))>)')
hexvals = regex.compile(r'^[<]?(0x[0-9a-f]+|[0-9]+)(:? \(-[0-9]+\))?[>]?$')

DIEs={}
level=0
last=0
if not os.path.exists(args.file):
	print(f"Input file '{args.file}' does not exist!", file=sys.stderr)
	sys.exit(1)
dwarfdump = subprocess.Popen(['dwarfdump', '-i', '-e', '-d', args.file], stdout=subprocess.PIPE)
while line := dwarfdump.stdout.readline().decode('utf-8'):
	if entry := entries.match(line):
		ID=int(entry.group(2), 0)
		DIEs[ID] = {}
		DIEs[ID]['tag'] = entry.group(3)
		DIEs[ID]['children'] = []

		# Nesting level
		l = int(entry.group(1))
		if l == 0:
			DIEs[ID]['parent'] = ID
		else:
			if l == level:
				parent = DIEs[last]['parent']
			elif l > level:
				parent = last
			elif l < level:
				last_parent = DIEs[last]['parent']
				parent = DIEs[last_parent]['parent']
			level = l
			# Set parent
			DIEs[ID]['parent'] = parent
			# In parent add child
			DIEs[parent]['children'].append(ID)
		last = ID

		# Additional attributes
		for attrib in attribs.finditer(entry.group(4)):
			value = attrib.group(3)
			if hexval := hexvals.match(value):
				value = int(hexval.group(1), 0)
			DIEs[ID][attrib.group(1)] = value

		# Reduce lines by combining declaration source
		if args.source and 'decl_file' in DIEs[ID] and 'decl_line' in DIEs[ID]:
			DIEs[ID]['decl'] = DIEs[ID]['decl_file'].split(" ", 1)[1] + ':' + str(DIEs[ID]['decl_line'])
			del DIEs[ID]['decl_file']
			del DIEs[ID]['decl_line']
			if 'decl_column' in DIEs[ID]:
				DIEs[ID]['decl'] += ':' + str(DIEs[ID]['decl_column'])
				del DIEs[ID]['decl_column']

def get_type(ID):
	if not 'type_string' in DIEs[ID]:
		identifier=''
		size = -1
		factor = 1
		while True:
			if DIEs[ID]['tag'] == 'structure_type':
				identifier += 'struct'
			elif DIEs[ID]['tag'] == 'union_type':
				identifier += 'union'
			elif DIEs[ID]['tag'] == 'enumeration_type':
				identifier += 'enum'
			elif DIEs[ID]['tag'] == 'typedef' and args.aliases:
				identifier += 'typedef'
			elif DIEs[ID]['tag'] == 'pointer_type':
				identifier += '*'
				size = -1
			elif DIEs[ID]['tag'] == 'array_type':
				for sub in DIEs[ID]['children']:
					lower = DIEs[sub].get('lower_bound', 0)
					upper = DIEs[sub].get('upper_bound', 0)
					num = upper - lower + 1
					identifier += '[' + str(num) + ']'
					if size == -1:
						factor *= num

			if 'name' in DIEs[ID] and args.aliases:
				identifier += ' ' + DIEs[ID]['name'] + '->'

			if 'byte_size' in DIEs[ID] and size == -1:
				size = DIEs[ID]['byte_size']

			if 'type' in DIEs[ID]:
				ID = DIEs[ID]['type']
			else:
				break;

		if 'encoding' in DIEs[ID]:
			identifier += DIEs[ID]['encoding']

		DIEs[ID]['type_string'] = identifier + ':' + str(size), factor * size

	return DIEs[ID]['type_string']


if args.extract == 'variables':
	locaddr = regex.compile(r'^.*: addr (0x[0-9a-f]+)$')
	for ID, DIE in DIEs.items():
		if DIE['tag'] == 'variable' and 'location' in DIE:
			if loc := locaddr.match(DIE['location']):
				addr = int(loc.group(1), 0)
				typename, size = get_type(DIE['type'])
				visibility = 'GLOBAL' if 'external' in DIE and DIE['external'][:3] == 'yes' else 'LOCAL '
				source = f" // {DIE['decl']}" if args.source else ''
				print(f"     0: {addr:016x} {size:>5} OBJECT  {visibility} DEFAULT  UND {DIE['name']} # {typename}{source}")
elif args.extract == 'datatypes':
	for ID, DIE in DIEs.items():
		if DIEs[ID]['tag'] == 'structure_type' and len(DIEs[ID]['children']) > 0:
			if args.source:
				print(f"// {DIE['decl']}")
			name = ' ' + DIEs[ID]['name'] if args.names else ''
			print(f"struct{name} ({DIEs[ID]['byte_size']}b):")
			for child in DIEs[ID]['children']:
				assert DIEs[child]['tag'] == 'member'
				name = ' ' + DIEs[child]['name'] if args.names else ''
				typename, size = get_type(DIEs[child]['type'])
				print(f"   {name} ({typename}) <{size}b> @ {DIEs[child]['data_member_location']}")
			print()

		if DIEs[ID]['tag'] == 'union_type' and len(DIEs[ID]['children']) > 0:
			if args.source:
				print(f"// {DIE['decl']}")
			name = ' ' + DIEs[ID]['name'] if args.names else ''
			print(f"union{name} ({DIEs[ID]['byte_size']}b):")
			for child in DIEs[ID]['children']:
				assert DIEs[child]['tag'] == 'member'
				name = ' ' + DIEs[child]['name'] if args.names else ''
				typename, size = get_type(DIEs[child]['type'])
				print(f"   {name} ({typename}) <{size}b>")
			print()

		if DIEs[ID]['tag'] == 'enumeration_type' and len(DIEs[ID]['children']) > 0:
			if args.source:
				print(f"// {DIE['decl']}")
			name = ' ' + DIEs[ID]['name'] if args.names else ''
			print(f"enum{name} <{DIEs[ID]['encoding']}:{DIEs[ID]['byte_size']}b>:")
			enums={}
			for child in DIEs[ID]['children']:
				assert DIEs[child]['tag'] == 'enumerator'
				enums[DIEs[child]['name']]=DIEs[child]['const_value']
			for k,v in dict(sorted(enums.items(), key=lambda item: item[1])).items():
				print(f'    {k} = {v}')
			print()


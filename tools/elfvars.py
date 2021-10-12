import sys
import xxhash
import argparse
import functools

from dwarfvars import DwarfVars

from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection
from elftools.elf.constants import P_FLAGS

def compare(a, b):
	if a['category'] != b['category']:
		return hash(a['category']) - hash(b['category'])
	elif a['value'] == b['value']:
		return hash(a['name']) - hash(b['name'])
	else:
		return a['value'] - b['value']

if __name__ == '__main__':
	# Arguments
	parser = argparse.ArgumentParser(prog='PROG')
	parser.add_argument('-a', '--aliases', action='store_true', help='Include aliases (typedefs)')
	parser.add_argument('-v', '--verbose', action='store_true', help='Verbose output')
	parser.add_argument('-n', '--names', action='store_true', help='Include names (complex types / members)')
	parser.add_argument('file', metavar="FILE", help="ELF file with debug information")
	args = parser.parse_args()
	
	with open(args.file, 'rb') as f:
		variables = []
		elffile = ELFFile(f)
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

		dwarf = DwarfVars(args.file, aliases = args.aliases, names = args.names)
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
			print(hash.hexdigest(), cat, output if args.verbose else '')




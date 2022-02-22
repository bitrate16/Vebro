MIT = """
MIT License

Copyright (c) 2022 bitrate16

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
"""

EULA = """
End User License Agreement (EULA)

You have the right to distribute and use this software in accordance with the
attached license and agree that you are solely responsible for the use of this
software and developer is not responsible for the use of this software, possible
damage to you, your hardware or software or data, and also to shadertoy.com
service.

This agreement prohibits the use of this software for the purpose of
intentionally damaging the service shadertoy.com in any way or knowingly
circumventing the current license agreements or copyrights on the source codes
of shaders and related software provided by the service and its users, as well
as images, audio, video and other media files and data that can be used as input
to shaders.

This software does not give you the right to use or distribute the source codes
of user shaders without agreeing to the license that is applied by default for
each shader (https://www.shadertoy.com/terms) and other licenses specified by
users in the description of the shader or its source code.

This software does not give you the right to use the downloaded images, audio,
video and other media and data without agreeing to the rules and license for
distribution and use (https://www.shadertoy.com/terms) of the shadertoy.com
service.

This software is intended for personal, non-commercial use, subject to the terms
described.

If you agree with the terms of this agreement, run the program with:

--eula=true
"""

# This utility downloads shader from shadertoy and saves it as single pack in given folder

import os
import json
import string
import argparse
import requests
import traceback

import urllib.parse
import urllib.request


SHADER_HEADER = """#version 330 core
uniform vec3 iResolution;
uniform float iTime;
uniform float iTimeDelta;
uniform int iFrame;
uniform float iChannelTime[4];
uniform vec3 iChannelResolution[4];
uniform vec4 iMouse;
uniform sampler2D iChannel0;
uniform sampler2D iChannel1;
uniform sampler2D iChannel2;
uniform sampler2D iChannel3;
uniform vec4 iDate;
uniform float iSampleRate;
out vec4 out_FragColor;"""

SHADER_MAIN = """void main(){vec4 color=vec4(0.0,0.,0.,1.);mainImage(color,gl_FragCoord.xy);color.rgb=clamp(color.rgb,0.,1.);color.w=1.0;out_FragColor=color;}"""

DEBUG = True


def get_shader_info(shader_id):
	"""
	Requests JSON data by shader ID
	"""
	
	try:
		
		# GET

		headers = {
			'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:89.0) Gecko/20100101 Firefox/89.0',
			'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8',
			'Accept-Language': 'en-US,en;q=0.5',
			'DNT': '1',
			'Connection': 'keep-alive',
			'Upgrade-Insecure-Requests': '1',
			'Pragma': 'no-cache',
			'Cache-Control': 'no-cache',
		}

		response = requests.get('https://www.shadertoy.com/shadertoy', headers=headers)


		# POST

		headers = {
			'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:89.0) Gecko/20100101 Firefox/89.0',
			'Accept': '*/*',
			'Accept-Language': 'en-US,en;q=0.5',
			'Referer': f'https://www.shadertoy.com/view/{shader_id}',
			'Content-Type': 'application/x-www-form-urlencoded',
			'Pragma': 'no-cache',
			'Cache-Control': 'no-cache',
			'Origin': 'https://www.shadertoy.com',
			'DNT': '1',
			'Connection': 'keep-alive',
		}

		data = {
		's': '{ "shaders" : ["' + shader_id + '"] }',
		'nt': '1',
		'nl': '1',
		'np': '1'
		}

		response = requests.post('https://www.shadertoy.com/shadertoy', headers=headers, cookies=response.cookies, data=data)
		
		if response.status_code != 200 or len(response.content) == 0 or len(response.json()) == 0:
			if DEBUG:
				print(f'Invalid shader id "{shader_id}"')
			
			return None
		
		return response.json()
		
	except:
		
		if DEBUG:
			traceback.print_exc()
		
		return None

def parse_shader_id(url):
	
	"""
	Parse shader id from url similar to https://www.shadertoy.com/view/BAOBAB.
	Additinaly performs weak check of shader url.
	"""
	
	try:
		parsed = urllib.parse.urlparse(url)
		if not parsed.path.startswith('/view/'):
			return None
		
		return parsed.path[6:]
		
	except:
		if DEBUG:
			traceback.print_exc()
		
		return None

def get_obj_with_attr(list, name, value):
	for obj in list:
		if obj[name] == value:
			return obj
	return None

# Arguments
parser = argparse.ArgumentParser(description='Download shadertoy.com shader')

group = parser.add_mutually_exclusive_group(required=True)
group.add_argument('--url', dest='url', default=None, help='url of the shader, example https://www.shadertoy.com/view/AMSyft')
group.add_argument('--id', dest='id', default=None, help='id of the shader, example AMSyft')
group.add_argument('--clipboard-url', default=False, dest='clipboard_url', action='store_true', help='get shader url from clipboard, requires pyperclip module')
group.add_argument('--clipboard-id', default=False, dest='clipboard_id', action='store_true', help='get shader id from clipboard, requires pyperclip module')

parser.add_argument('--output', dest='output', default=None, help='output folder and pack name')
parser.add_argument('--outside', dest='outside', default=None, help='put pack.json outside of the folder')

parser.add_argument('--eula', dest='eula', type=str, default=None, help='End User License Agreement')
parser.add_argument('--license', dest='license', action='store_true', default=None, help='License Agreement')

args = parser.parse_args()

# Check EULA
if args.eula != 'true' and args.eula != 'yes':
	print(EULA)
	exit(0)

if args.license:
	print(MIT)
	exit(0)


if args.url:
	shader_url = args.url.strip()
	shader_id = parse_shader_id(shader_url)

if args.id:
	shader_url = 'https://www.shadertoy.com/view/' + args.id.strip()
	shader_id = args.id.strip()

if args.clipboard_url:
	try:
		import pyperclip
	except:
		print("Install pyperclip module with 'pip install pyperclip'")
		exit(0)
	
	shader_url = pyperclip.paste().strip()
	shader_id = parse_shader_id(shader_url)

if args.clipboard_id:
	try:
		import pyperclip
	except:
		print("Install pyperclip module with 'pip install pyperclip'")
		exit(0)
	
	shader_url = 'https://www.shadertoy.com/view/' + pyperclip.paste().strip()
	shader_id = pyperclip.paste().strip()


# Output folder
if args.output:
	output = args.output.strip()
else:
	output = None # Get from response

if not shader_id:
	exit(0)

if DEBUG:
	print('Shader id:', shader_id)

shader_info = get_shader_info(shader_id)
if not shader_info:
	exit(0)

os.makedirs(output, exist_ok=True)

# Output json
pack_json = {}

# Shader info
shader = shader_info[0]

# Get shader name and escape everything except ascii path safe
if not output:
	path_safe = string.digits + string.ascii_letters + '_-.'
	output = ''.join([ c for c in shader['info']['name'] if c in path_safe ])
	
	if not output:
		output = shader_id
	
# Save description data
pack_json['Author'] = shader['info']['username']
pack_json['Name'] = shader['info']['name']
pack_json['Id'] = shader['info']['id']
pack_json['Source'] = 'https://www.shadertoy.com/view/' + shader['info']['id']

# Common and other objects
common  = get_obj_with_attr(shader['renderpass'], 'name', 'Common')
main    = get_obj_with_attr(shader['renderpass'], 'name', 'Image')
buffera = get_obj_with_attr(shader['renderpass'], 'name', 'Buffer A') or get_obj_with_attr(shader['renderpass'], 'name', 'Buf A')
bufferb = get_obj_with_attr(shader['renderpass'], 'name', 'Buffer B') or get_obj_with_attr(shader['renderpass'], 'name', 'Buf B')
bufferc = get_obj_with_attr(shader['renderpass'], 'name', 'Buffer C') or get_obj_with_attr(shader['renderpass'], 'name', 'Buf C')
bufferd = get_obj_with_attr(shader['renderpass'], 'name', 'Buffer D') or get_obj_with_attr(shader['renderpass'], 'name', 'Buf D')

buffers = [ buffera, bufferb, bufferc, bufferd ]

# Common source code
common_source = '' if common is None else common['code']

# Write Main shader properties
if main:
	main_source = '' if main is None else main['code']
	
	# Write source of Main.glsl
	with open(f'{output}/Main.glsl', 'w') as f:
		
		if DEBUG:
			print(f'Main shader path: {output}/Main.glsl')
		
		f.write(SHADER_HEADER)
		f.write('\n')
		f.write(common_source)
		f.write('\n')
		f.write(main_source)
		f.write('\n')
		f.write(SHADER_MAIN)
	
	# Collect inputs
	inputs = [ None ] * 4
	for i in range(4):
		input_info = get_obj_with_attr(main['inputs'], 'channel', i)
		
		if not input_info:
					
			if DEBUG:
				print('Main Input', i, 'is None')
			
			continue
		
		inputs[i] = {}
		
		# Detect buffer type
		if input_info['type'] == 'buffer':
			
			if input_info['filepath'].find('buffer00') != -1:
				inputs[i]['type'] = 'BufferA'
			if input_info['filepath'].find('buffer01') != -1:
				inputs[i]['type'] = 'BufferB'
			if input_info['filepath'].find('buffer02') != -1:
				inputs[i]['type'] = 'BufferC'
			if input_info['filepath'].find('buffer03') != -1:
				inputs[i]['type'] = 'BufferD'
					
			if DEBUG:
				print('Main Input', i, 'type:', inputs[i]['type'])
		
		# Get image from local storage
		elif input_info['type'] == 'texture':
			inputs[i]['type'] = 'Image'
			inputs[i]['path'] = input_info['filepath'][1:]
			
			# Download texture
			media_directory = inputs[i]['path']
			media_directory = media_directory[:media_directory.rfind('/')]
			os.makedirs(f'{output}/{media_directory}', exist_ok=True)
					
			if DEBUG:
				print('Main Input', i, 'type:', inputs[i]['type'])
				print('Downloading', f'https://www.shadertoy.com/{inputs[i]["path"]}')
			
			urllib.request.urlretrieve(f'https://www.shadertoy.com/{inputs[i]["path"]}', f'{output}/{inputs[i]["path"]}')
		
		# TODO: Add other input types
		elif DEBUG:
			print('Main shader Input', i, 'has unsupported type:', input_info['type'])
			
	pack_json['Main'] = {
		'inputs': inputs,
		'path': 'Main.glsl'
	}

else:
	
	if DEBUG:
		print('Missing Main shader')

# Write buffer shaders
for buffer_name, buffer in zip([ 'BufferA', 'BufferB', 'BufferC', 'BufferD' ], buffers):
	if buffer:
		buffer_source = '' if buffer is None else buffer['code']
		
		# Write source of Main.glsl
		with open(f'{output}/{buffer_name}.glsl', 'w') as f:
			
			if DEBUG:
				print(f'{buffer_name} shader path: {output}/{buffer_name}.glsl')
			
			f.write(SHADER_HEADER)
			f.write('\n')
			f.write(common_source)
			f.write('\n')
			f.write(buffer_source)
			f.write('\n')
			f.write(SHADER_MAIN)
		
		# Collect inputs
		inputs = [ None ] * 4
		for i in range(4):
			input_info = get_obj_with_attr(buffer['inputs'], 'channel', i)
		
			if not input_info:
				
				if DEBUG:
					print(buffer_name, 'Input', i, 'is None')
				
				continue
		
			inputs[i] = {}
			
			# Detect buffer type
			if input_info['type'] == 'buffer':
				
				if input_info['filepath'].find('buffer00') != -1:
					inputs[i]['type'] = 'BufferA'
				if input_info['filepath'].find('buffer01') != -1:
					inputs[i]['type'] = 'BufferB'
				if input_info['filepath'].find('buffer02') != -1:
					inputs[i]['type'] = 'BufferC'
				if input_info['filepath'].find('buffer03') != -1:
					inputs[i]['type'] = 'BufferD'
					
				if DEBUG:
					print(buffer_name, 'Input', i, 'type:', inputs[i]['type'])
			
			# Get image from local storage
			elif input_info['type'] == 'texture':
				inputs[i]['type'] = 'Image'
				inputs[i]['path'] = input_info['filepath'][1:]
				
				# Download texture
				media_directory = inputs[i]['path']
				media_directory = media_directory[:media_directory.rfind('/')]
				os.makedirs(f'{output}/{media_directory}', exist_ok=True)
				
				if DEBUG:
					print(buffer_name, 'Input', i, 'type:', inputs[i]['type'])
					print('Downloading', f'https://www.shadertoy.com/{inputs[i]["path"]}')
				
				urllib.request.urlretrieve(f'https://www.shadertoy.com/{inputs[i]["path"]}', f'{output}/{inputs[i]["path"]}')
			
			# TODO: Add other input types
			elif DEBUG:
				print(buffer_name, 'shader Input', i, 'has unsupported type:', input_info['type'])
			
		pack_json[buffer_name] = {
			'inputs': inputs,
			'path': f'{buffer_name}.glsl'
		}

	else:
		
		if DEBUG:
			print(f'Missing {buffer_name} shader')

# Write pack
with open(f'{output}/{output}.json', 'w') as f:
	f.write(json.dumps(pack_json, indent=4, sort_keys=True))



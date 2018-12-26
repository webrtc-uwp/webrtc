#!/usr/bin/python
#
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"This script is used to run a perl script."

import optparse
import subprocess
import sys

parser = optparse.OptionParser()
parser.description = __doc__
parser.add_option('-s', '--script', help='path to a perl script.')
parser.add_option('-i', '--input', help='file passed to stdin.')
parser.add_option('-o', '--output', help='file saved from stdout.')
parser.add_option('-p', '--path', help='path')
parser.add_option('-l', '--include', help='include')
parser.add_option('-a', '--arch', help='cpu architecture')


options, args = parser.parse_args()
if (not options.script or not options.arch or not options.input or not options.output):
  parser.error('Must specify arguments for script, arch, input and output.')
  sys.exit(1)

pairs = open(options.path).read()[:-2].split('\0')
kvs = [item.split('=', 1) for item in pairs]
if options.arch == 'arm':
  arm_compiler = 'armasm.exe'
else:
  arm_compiler = 'armasm64.exe'
asm_type = 'armasm'
with open(options.output, 'w') as fo, open(options.input, 'r') as fi:
  subprocess.check_call(['perl', options.script, '-arch', options.arch, '-as-type', asm_type, '-force-thumb', '--', arm_compiler, '-oldit', '-I' + options.include, options.input, '-o', options.output ], env=dict(kvs))

sys.exit(0)

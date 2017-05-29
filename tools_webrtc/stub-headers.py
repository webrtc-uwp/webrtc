#!/usr/bin/env python

import argparse
import os
import sys
import re
import textwrap


def _calc_header_guard(dirname, filename):
  result = '%s_%s' % (dirname, filename)
  result = ''.join(c if c.isalnum() else '_' for c in result.lower())
  return result.upper() + '_'


def convert_to_stub(dirname, filename, new_path, issue_number):
  """Converts a file in a dir to be just forwarding to a similarly named
  (header) file in another path."""

  expected_header_guard = _calc_header_guard(dirname, filename)
  #print 'Processing: %s header guard: %s' % (filename, expected_header_guard)

  file_path = os.path.join(dirname, filename)
  with open(file_path) as f:
    content = f.read()

    source_matcher = re.compile(r'(.*#define %s\n)(.*)(#endif.*)' %
                                expected_header_guard, re.DOTALL)
    match = re.search(source_matcher, content)

    if match:
      #print 'Found match in %s' % filename
      temp_file = filename + '.new'
      with open(temp_file, 'w') as out:
        out.write(match.group(1))
        out.write(textwrap.dedent("""\n
        // This header is deprecated and is just left here temporarily during
        // refactoring. See https://bugs.webrtc.org/%d for more details.\n"""
        % issue_number))
        out.write('#include "%s/%s"\n' % (new_path, filename))
        out.write('\n')
        out.write(match.group(3))
      os.rename(temp_file, file_path)
    else:
      print 'WARNING: Couldn\'t find expected header guard in %s' % file_path


def main():
  if os.path.basename(os.getcwd()) != 'src':
    print 'Make sure you run this script with cwd being src/'
    sys.exit(1)

  p = argparse.ArgumentParser()
  p.add_argument('-s', '--source-dir',
                 help=('Source directory of headers to process.'))
  p.add_argument('-d', '--dest-dir',
                 help=('Destination dir of where headers have been moved.'))
  p.add_argument('-i', '--issue-number', type=int,
                 help=('Issue number at bugs.webrtc.org to reference.'))
  p.add_argument('--dry-run', action='store_true', default=False,
                 help=('Don\'t perform any modifications.'))
  opts = p.parse_args()
  if not opts.source_dir or not opts.dest_dir or not opts.issue_number:
    print 'Please provide the -s, -d and -i flags.'
    sys.exit(2)

  print 'Listing headers in %s' % opts.source_dir
  for dirname, _, file_list in os.walk(opts.source_dir):
    for filename in file_list:
      if filename.endswith('.h') and filename != 'sslroots.h':
        convert_to_stub(dirname, filename, opts.dest_dir, opts.issue_number)


if __name__ == '__main__':
  sys.exit(main())

#!/usr/bin/python

# $Id$
## @file
# A python 2.x script to create a .qhp file outof a given htmlhelp
# folder. Lots of things about the said folder is assumed. Please
# read the code and inlined comments.
#
# Copyright (C) 2006-2020 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.


import sys, getopt
import os.path
import re
import codecs
import logging

# number of opened and not yet closed section tags of toc section
open_section_tags = 0

# find the png files under /images folder and create a part of the
# qhelp project file with <file> tags
def create_image_list(folder):
    image_folder_name = 'images'
    image_files_list = []
    # Look for 'images' sub folder
    subdirs = [x[0] for x in os.walk(folder)]
    full_folder_path = os.path.join(folder, image_folder_name)
    if full_folder_path not in subdirs:
        logging.error('Image subfolder "%s" is not found under "%s".', image_folder_name, folder)
        return image_files_list;
    png_files = []
    for f in os.listdir(full_folder_path):
        png_files.append(image_folder_name + '/' + f)
        image_files_list.append('<file>images/' + f + '</file>')
    return image_files_list

# open htmlhelp.hhp files and read the list of html files from there
def create_html_list(folder):
    file_name = 'htmlhelp.hhp'
    html_files_list = []
    if not file_name in os.listdir(folder):
        logging.error('Could not find the file "%s" in "%s"', file_name, folder)
        return html_files_list
    full_path = os.path.join(folder, 'htmlhelp.hhp')
    file = open(full_path, "r")
    lines = file.readlines()
    file.close()
    # first search for the [FILES] marker then collect .html lines
    marker_found = 0
    for line in lines:
        if '[FILES]' in line:
            marker_found = 1
            continue
        if marker_found == 0:
            continue
        if '.html' in line:
            html_files_list.append('<file>' + line.strip('\n') + '</file>')
    return html_files_list


def create_files_section(folder):
    files_section_lines = ['<files>']
    files_section_lines += create_image_list(folder)
    files_section_lines += create_html_list(folder)
    files_section_lines.append('</files>')
    return files_section_lines

def parse_param_tag(line):
    label = 'value="'
    start = line.find(label);
    if start == -1:
        return ''
    start +=  len(label)
    end = line.find('"', start)
    if end == -1:
        return '';
    return line[start:end]

# look at next two lines. they are supposed to look like the following
#      <param name="Name" value="Oracle VM VirtualBox">
#      <param name="Local" value="index.html">
# parse out value fields and return
# title="Oracle VM VirtualBox" ref="index.html
def parse_object_tag(lines, index):
    result=''
    if index + 2 > len(lines):
        logging.warning('Not enough tags after this one "%s"',lines[index])
        return result
    if not re.match(r'^\s*<param', lines[index + 1], re.IGNORECASE) or \
       not re.match(r'^\s*<param', lines[index + 2], re.IGNORECASE):
        logging.warning('Skipping the line "%s" since next two tags are supposed to be param tags',  lines[index])
        return result

    title = parse_param_tag(lines[index + 1])
    ref = parse_param_tag(lines[index + 2])
    global open_section_tags
    if title and ref:
        open_section_tags += 1
        result = '<section title="' + title + '" ref="' + ref + '">'
    else:
        logging.warning('Title or ref part is empty for the tag "%s"', lines[index])
    return result

# parse any string other than staring with <OBJECT
# decide if <session tag should be closed
def parse_non_object_tag(lines, index):
    if index + 1 > len(lines):
        return ''
    global open_section_tags
    if open_section_tags <= 0:
        return ''
    # replace </OBJECT with </section only if the next tag is not <UL
    if re.match(r'^\s*</OBJECT', lines[index], re.IGNORECASE):
        if not re.match(r'^\s*<UL', lines[index + 1], re.IGNORECASE):
            open_section_tags -= 1
            return '</section>'
    elif re.match(r'^\s*</UL', lines[index], re.IGNORECASE):
        open_section_tags -= 1
        return '</section>'
    return ''

def parse_line(lines, index):
    result=''

    # if the line starts with <OBJECT
    if re.match(r'^\s*<OBJECT', lines[index], re.IGNORECASE):
        result = parse_object_tag(lines, index)
    else:
        result = parse_non_object_tag(lines, index)
    return result

# parse toc.hhc file. assuming all the relevant informations
# is stored in tags and attributes. data "whatever is outside of
# <... > pairs is filtered out. we also assume < ..> are not nested
# and each < matches to a >
def parse_toc(folder):
    toc_file = 'toc.hhc'
    content = [x[2] for x in os.walk(folder)]
    if toc_file not in content[0]:
        logging.error('Could not find toc file "%s" under "%s"', toc_file, folder)
        return
    full_path = os.path.join(folder, toc_file)
    file = codecs.open(full_path, encoding='iso-8859-1')
    content = file.read()
    file.close()
    # convert the file string into a list of tags there by eliminating whatever
    # char reside outside of tags.
    char_pos = 0
    tag_list = []
    while char_pos < len(content):
        start = content.find('<', char_pos)
        if start == -1:
            break
        end = content.find('>', start)
        if end == -1 or end >= len(content) - 1:
            break
        char_pos = end
        tag_list.append(content[start:end +1])

    # # insert new line chars. to make sure each line includes at most one tag
    # content = re.sub(r'>.*?<', r'>\n<', content)
    # lines = content.split('\n')
    toc_string_list = ['<toc>']
    index = 0
    for tag in tag_list:
        str = parse_line(tag_list, index)
        if str:
            toc_string_list.append(str)
        index += 1
    toc_string_list.append('</toc>')
    toc_string = '\n'.join(toc_string_list)

    return toc_string_list

def usage(arg):
    print 'test.py -d <helphtmlfolder> -o <outputfilename>'
    sys.exit(arg)

def main(argv):
    helphtmlfolder = ''
    output_filename = ''
    try:
        opts, args = getopt.getopt(sys.argv[1:],"hd:o:")
    except getopt.GetoptError as err:
        print err
        usage(2)
    for opt, arg in opts:
        if opt == '-h':
            usage(0)
        elif opt in ("-d"):
            helphtmlfolder = arg
        elif opt in ("-o"):
             output_filename = arg

    # check supplied helphtml folder argument
    if not helphtmlfolder:
        logging.error('No helphtml folder is provided. Exiting')
        usage(2)
    if not os.path.exists(helphtmlfolder):
        logging.error('folder "%s" does not exist. Exiting', helphtmlfolder)
        usage(2)
    helphtmlfolder = os.path.normpath(helphtmlfolder)

    # check supplied output file name
    if not output_filename:
        logging.error('No filename for output is given. Exiting')
        usage(2)

    out_xml_lines = ['<?xml version="1.0" encoding="UTF-8"?>', \
                     '<QtHelpProject version="1.0">' , \
                     '<namespace>org.qt-project.simpletextviewer</namespace>', \
                     '<virtualFolder>doc</virtualFolder>', \
                     '<filterSection>']
    out_xml_lines += parse_toc(helphtmlfolder) + create_files_section(helphtmlfolder)
    out_xml_lines += ['</filterSection>', '</QtHelpProject>']

    out_file = open(output_filename, 'w')
    out_file.write('\n'.join(out_xml_lines).encode('utf8'))
    out_file.close()

if __name__ == '__main__':
    main(sys.argv[1:])

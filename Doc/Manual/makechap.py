#!/usr/bin/env python

###############################################################################
# Takes a chapter as input and adds internal links and numbering to all
# of the H1, H2, H3, H4 and H5 sections.
#
# Every heading HTML tag (H1, H2 etc) is given an autogenerated name to link
# to. However, if the name is not an autogenerated name from a previous run,
# it will be kept. If it is autogenerated, it might change on subsequent runs
# of this program. Thus if you want to create links to one of the headings,
# then change the heading link name to something that does not look like an
# autogenerated link name.
###############################################################################

import sys
import re
import string

###############################################################################
# Functions
###############################################################################

# Regexs for <a name="..."></a>
alink = re.compile(r"<a *name *= *\"(.*)\">.*</a>", re.IGNORECASE)
heading = re.compile(r"(_nn\d)", re.IGNORECASE)

def getheadingname(m):
    autogeneratedheading = True;
    if m.group(1) != None:
        amatch = alink.match(m.group(1))
        if amatch:
            # A non-autogenerated heading - keep it
            headingname = amatch.group(1)
            autogeneratedheading = heading.match(headingname)
    if autogeneratedheading:
        # The heading name was either non-existent or autogenerated,
        # We can create a new heading / change the existing heading
        headingname = "%s_nn%d" % (filenamebase, nameindex)
    return headingname

# Return heading - 1.1. Introduction in the examples below:
# old style example: <H2><a name="Preface_nn2"></a>1.1 Introduction</H2>
# new style example: <H2><a name="Preface_nn2">1.1 Introduction</a></H2>
def getheadingtext(m, s):
    prevheadingtext_newstyle = m.group(2)
    prevheadingtext_oldstyle = m.group(3)
    if prevheadingtext_oldstyle is None or prevheadingtext_newstyle is None:
        raise RuntimeError("Ill-formed heading in line:\n%s" % s)
    if len(prevheadingtext_oldstyle) == 0 and len(prevheadingtext_newstyle) == 0:
        raise RuntimeError("No heading text in line:\n%s" % s)
    if len(prevheadingtext_oldstyle) > 0 and len(prevheadingtext_newstyle) > 0:
        raise RuntimeError("Two heading texts, only one should be specified in line:\n%s" % s)
    prevheadingtext = prevheadingtext_oldstyle if len(prevheadingtext_oldstyle) > 0 else prevheadingtext_newstyle
    return prevheadingtext

###############################################################################
# Main program
###############################################################################

if len(sys.argv) != 3:
    print "usage: makechap.py filename num"
    sys.exit(1)

filename = sys.argv[1]
filenamebase = string.split(filename,".")[0]
num      = int(sys.argv[2])

section = 0
subsection = 0
subsubsection = 0
subsubsubsection = 0
nameindex = 0

name = ""

# Regexs for <h1>,... <h5> sections

h1 = re.compile(r".*?<H1>(<a.*?>\s*[\d\.\s]*(.*?)</a>)*[\d\.\s]*(.*?)</H1>", re.IGNORECASE)
h2 = re.compile(r".*?<H2>(<a.*?>\s*[\d\.\s]*(.*?)</a>)*[\d\.\s]*(.*?)</H2>", re.IGNORECASE)
h3 = re.compile(r".*?<H3>(<a.*?>\s*[\d\.\s]*(.*?)</a>)*[\d\.\s]*(.*?)</H3>", re.IGNORECASE)
h4 = re.compile(r".*?<H4>(<a.*?>\s*[\d\.\s]*(.*?)</a>)*[\d\.\s]*(.*?)</H4>", re.IGNORECASE)
h5 = re.compile(r".*?<H5>(<a.*?>\s*[\d\.\s]*(.*?)</a>)*[\d\.\s]*(.*?)</H5>", re.IGNORECASE)

data = open(filename).read()            # Read data
open(filename+".bak","w").write(data)   # Make backup

lines = data.splitlines()
result = [ ] # This is the result of postprocessing the file
index = "<!-- INDEX -->\n<div class=\"sectiontoc\">\n" # index contains the index for adding at the top of the file. Also printed to stdout.

skip = 0
skipspace = 0

for s in lines:
    if s == "<!-- INDEX -->":
        if not skip:
            skip = 1
        else:
            skip = 0
        continue;
    if skip:
        continue

    if not s and skipspace:
        continue

    if skipspace:
        result.append("")
        result.append("")
        skipspace = 0
    
    m = h1.match(s)
    if m:
        prevheadingtext = getheadingtext(m, s)
        nameindex += 1
        headingname = getheadingname(m)
        result.append("""<H1><a name="%s">%d %s</a></H1>""" % (headingname,num,prevheadingtext))
        result.append("@INDEX@")
        section = 0
        subsection = 0
        subsubsection = 0
        subsubsubsection = 0
        name = prevheadingtext
        skipspace = 1
        continue
    m = h2.match(s)
    if m:
        prevheadingtext = getheadingtext(m, s)
        nameindex += 1
        section += 1
        headingname = getheadingname(m)
        result.append("""<H2><a name="%s">%d.%d %s</a></H2>""" % (headingname,num,section, prevheadingtext))

        if subsubsubsection:
            index += "</ul>\n"
        if subsubsection:
            index += "</ul>\n"
        if subsection:
            index += "</ul>\n"
        if section == 1:
            index += "<ul>\n"

        index += """<li><a href="#%s">%s</a>\n""" % (headingname,prevheadingtext)
        subsection = 0
        subsubsection = 0
        subsubsubsection = 0
        skipspace = 1        
        continue
    m = h3.match(s)
    if m:
        prevheadingtext = getheadingtext(m, s)
        nameindex += 1
        subsection += 1
        headingname = getheadingname(m)
        result.append("""<H3><a name="%s">%d.%d.%d %s</a></H3>""" % (headingname,num,section, subsection, prevheadingtext))

        if subsubsubsection:
            index += "</ul>\n"
        if subsubsection:
            index += "</ul>\n"
        if subsection == 1:
            index += "<ul>\n"

        index += """<li><a href="#%s">%s</a>\n""" % (headingname,prevheadingtext)
        subsubsection = 0
        skipspace = 1        
        continue
    m = h4.match(s)
    if m:
        prevheadingtext = getheadingtext(m, s)
        nameindex += 1
        subsubsection += 1

        headingname = getheadingname(m)
        result.append("""<H4><a name="%s">%d.%d.%d.%d %s</a></H4>""" % (headingname,num,section, subsection, subsubsection, prevheadingtext))

        if subsubsubsection:
            index += "</ul>\n"
        if subsubsection == 1:
            index += "<ul>\n"

        index += """<li><a href="#%s">%s</a>\n""" % (headingname,prevheadingtext)
        subsubsubsection = 0
        skipspace = 1        
        continue
    m = h5.match(s)
    if m:
        prevheadingtext = getheadingtext(m, s)
        nameindex += 1
        subsubsubsection += 1
        headingname = getheadingname(m)
        result.append("""<H5><a name="%s">%d.%d.%d.%d.%d %s</a></H5>""" % (headingname,num,section, subsection, subsubsection, subsubsubsection, prevheadingtext))

        if subsubsubsection == 1:
            index += "<ul>\n"

        index += """<li><a href="#%s">%s</a>\n""" % (headingname,prevheadingtext)
        skipspace = 1
        continue
    
    result.append(s)

if subsubsubsection:
    index += "</ul>\n"

if subsubsection:
    index += "</ul>\n"

if subsection:
    index += "</ul>\n"

if section:
    index += "</ul>\n"

index += "</div>\n<!-- INDEX -->\n"

data = "\n".join(result)

data = data.replace("@INDEX@",index) + "\n";

# Write the file back out
open(filename,"w").write(data)

# Print the TOC data to stdout correcting the anchor links for external referencing

index = index.replace("<li><a href=\"#","<li><a href=\"%s#" % filename)
print """<h3><a href="%s#%s">%d %s</a></h3>\n""" % (filename,filenamebase,num,name)
print index


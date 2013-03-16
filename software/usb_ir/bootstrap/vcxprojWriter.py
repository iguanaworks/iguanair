import os
import re
import sys
import shutil
import filecmp
import tempfile
import uuid

# grab the passed arguments
slnName = os.environ['PROJECT_NAME']
destDir = os.environ['DEST_DIR']
csPath = os.environ['CSPATH']
ipAddr = os.environ['TARGET_IP']

# compute the include path we want
path = None
for name in os.listdir(os.path.dirname(os.environ['CSPATH'])):
    location = os.path.join(os.path.dirname(os.environ['CSPATH']), name, 'libc', 'usr', 'include')
    if 'linux' in name and os.path.isdir(location):
        path = location
        break
incPath = ';'.join((path, os.path.join(os.path.dirname(os.environ['QTPATH']), 'include')))

projName = sys.argv[1]
names = list(sys.argv[2:])

replacements = [
 { 'before' : '__PROJECT_NAME__',
   'after'  : projName},
 { 'before' : '__IP_ADDR__',
   'after'  : ipAddr},
 { 'before' : '__CS_PATH__',
   'after'  : csPath},
 { 'before' : '__JOM_PATH__',
   'after'  : os.path.join(os.path.dirname(__file__), 'jom.exe')}
]

# include known files into the names list
names.append('../%s/CMakeLists.txt' % projName)

# group the incoming files into their types based on extension
files = []
while len(names):
    name = names.pop(0)
    label = 'None'
    ext = os.path.splitext(name)[1].lower()
    if ext in ('.cpp', '.cxx'):
        label = 'ClCompile'
# TODO: make the paths relative!
    files.append({ 'path'  : name.replace('\\', '/'),
                   'label' : label })

    # define what type of file it is
    if os.path.basename(name).startswith('moc_'):
        files[-1]['filter'] = 'Generated Files'
    elif ext in ('.cpp', '.cxx'):
        files[-1]['filter'] = 'Source Files'
    elif ext in ('.h',):
        files[-1]['filter'] = 'Header Files'
    elif os.path.basename(name) != 'CMakeLists.txt':
        files[-1]['filter'] = 'Resource Files'

# read in the contents of the current solution file to grab the UUID we want
projPat = re.compile('Project\("{(?P<uuid>.*)}"\) = "(?P<name>.*)", "(?P<vcxproj>.*)", "{(?P<uuid2>.*)}"')
projects = {}
configs = []
try:
    input = open(os.path.join(destDir, '%s.sln' % slnName), 'r')
except IOError:
    input = open(os.path.join(os.path.dirname(__file__), 'src', 'ArmBase.sln'), 'r')

inPost = False    
tempSln = tempfile.NamedTemporaryFile(mode = 'w', delete = False)
for line in input:
    match = projPat.match(line)
    if match:
        project = match.groupdict()
        projects[project['name']] = project
#        if project['name'] == projName:
#            print project
    elif line.strip() == 'Global':
        if projName not in projects:
            projects[projName] = {
                'uuid'    : str(uuid.uuid4()).upper(),
                'uuid2'   : str(uuid.uuid4()).upper(),
                'name'    : projName,
                'vcxproj' : projName + '.vcxproj'
            }
            tempSln.write('Project("{%(uuid)s}") = "%(name)s", "%(vcxproj)s", "{%(uuid2)s}"\nEndProject\n' % projects[projName])
    elif line.strip().endswith('postSolution'):
        inPost = True
    elif line.strip() == 'EndGlobalSection':
        if inPost and projects[projName]['uuid2'] not in configs:
            tempSln.write("""		{%(uuid2)s}.Debug|Win32.ActiveCfg = Debug|Win32
		{%(uuid2)s}.Debug|Win32.Build.0 = Debug|Win32
		{%(uuid2)s}.Release|Win32.ActiveCfg = Release|Win32
		{%(uuid2)s}.Release|Win32.Build.0 = Release|Win32
""" % projects[projName])
        inPost = False
    elif inPost:
        uuid2 = line.strip().split('.', 1)[0][1:-1]
        if uuid2 not in configs:
            configs.append(uuid2)
    tempSln.write(line)
tempSln.close()

oldSln = os.path.join(destDir, '%s.sln' % slnName)
if not os.path.exists(oldSln) or \
   not filecmp.cmp(tempSln.name, oldSln):
    shutil.move(tempSln.name, oldSln)
    print 'Generated new %s.sln' % slnName
else:
    os.unlink(tempSln.name)


oldProj = os.path.join(destDir, '%s.vcxproj' % projName)


def writeModifiedVCFile(src, files, filtered = False):
    temp = tempfile.NamedTemporaryFile(mode = 'w', delete = False)
    with open(os.path.join(os.path.dirname(__file__), 'src', src), 'r') as input:
        for line in input:
            # some text replacement is supported here
            if line.strip() == '<ProjectGuid>{GUID_HERE}</ProjectGuid>':
                line = line.replace('GUID_HERE', projects[projName]['uuid'])
            elif line.strip() == '<AdditionalIncludeDirectories>__INCLUDES_HERE__</AdditionalIncludeDirectories>':
                line = line.replace('__INCLUDES_HERE__', incPath)

            temp.write(line)
            if line.strip() == '</ItemGroup>':
                temp.write("  <ItemGroup>\n")
                for object in files:
                    if filtered and 'filter' in object:
                        temp.write("    <%s Include=\"%s\">\n      <Filter>%s</Filter>\n    </%s>\n" % (object['label'], object['path'], object['filter'], object['label']))
                    else:
                        temp.write("    <%s Include=\"%s\" />\n" % (object['label'], object['path']))
                temp.write("  </ItemGroup>\n")
    temp.close()

    return temp.name

def writeModifiedXML(src):
    temp = tempfile.NamedTemporaryFile(delete = False)
    try:
        input = open(os.path.join(destDir, '%s-settings.xml' % slnName), 'r')
    except IOError:
        input = None
    if input:
        for line in input:
            if not line.startswith('</preferences>'):
                temp.write(line)
    else:
        temp.write("<preferences>\n")
    for configName in ('Debug', 'Release'):
        reps = replacements
        reps.append({ 'before' : '__CONFIGURATION_NAME__',
                      'after'  : configName })
        with open(os.path.join(os.path.dirname(__file__), 'src', src), 'r') as input:
            for line in input:
                for replace in reps:
                    line = line.replace(replace['before'], replace['after'])
                temp.write(line)
    temp.write("</preferences>\n")
    return temp.name
    
tempProj = writeModifiedVCFile('ArmBase.vcxproj', files, False)
tempFilt = writeModifiedVCFile('ArmBase.vcxproj.filters', files, True)

# replace the old project with the new one if it's different or missing
if not os.path.exists(oldProj) or \
   not filecmp.cmp(tempProj, oldProj):
    shutil.move(tempProj, oldProj)
    shutil.move(tempFilt, oldProj + '.filters')
    print 'Generated new %s' % os.path.basename(oldProj)
else:
    os.unlink(tempProj)
    os.unlink(tempFilt)

tempWGDB = writeModifiedXML('ArmSettings.xml')
shutil.move(tempWGDB, os.path.join(destDir, '%s-settings.xml' % slnName))

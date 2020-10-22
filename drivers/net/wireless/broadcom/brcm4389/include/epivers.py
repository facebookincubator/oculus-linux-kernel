#!/usr/bin/env python3
"""
Generate a .h file describing the branch or tag version.

The idea here is to generate a header file which characterizes the
current build as well as possible. Most data is derived from the tag
or branch name with an assist from the SCM tool. No version data can be
derived on ToT (trunk, master, ...)  so the date as of header generation
time is used instead.

This script will update its target file only if the output differs
unless the --force option is used.

Normally, branch/revision data is queried from the current workspace
which will almost always be present in developer scenarios. In automated
build situations there may be no SCM context available locally in which
case a repo (server) query is made to a passed-in URL. If the SCM_REFTIME
environment variable is present its value is used to control the repo
query, otherwise the HEAD revision is derived.

Each built product has the concept of a "key component". The generated
header can represent the state of just one component so the top-level
makefile decides what its key component should be and passes its path
and/or URL to the script.  The state of other components is not captured
directly but it should be possible to work back to them from the data
derived via the key component.

There should never be a situation in which neither workspace nor repo is
available to this script. In a customer source package neither will be
present but in those scenarios the generated file is provided as a static
header and the generator script is not even packaged. Thus, if the script
is present at all a workspace and/or repo should be available to it.
"""

# No copyright needed in this script because it doesn't ship. Avoid
# confusion with the one in the header template.
# <<Broadcom-WL-IPTag/Secret:>>

import argparse
import json
import logging
import os
import subprocess
import sys
import tempfile
import time

# Use a pre-expanded copyright because this file may be generated
# outside of or subsequent to the normal copyright expansion process.
HEADER_TEMPLATE = """
/*
 * Copyright (C) {year}, {corp}.
 *
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 *
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 *
 *
 * <<{corp}-WL-IPTag/Open:>>
 *
*/

#ifndef _epivers_h_
#define _epivers_h_

#define EPI_MAJOR_VERSION	{epi_major_version}

#define EPI_MINOR_VERSION	{epi_minor_version}

#define EPI_RC_NUMBER		{epi_rc_number}

#define EPI_INCREMENTAL_NUMBER	{epi_incremental_number}

#define EPI_BUILD_NUMBER	{epi_build_number}

#define EPI_VERSION		{epi_version}

#define EPI_VERSION_NUM		{epi_version_num}

#define EPI_VERSION_DEV		{epi_version_dev}

/* Driver Version String, ASCII, 32 chars max */
#define EPI_VERSION_STR		"{epi_compound} ({scm_state})"

#endif /* _epivers_h_ */
"""

NOW = time.time()
SVN_LCR = 'Last Changed Rev:'
TRACE_LOGGER = logging.getLogger('trace')

def svn_urlparse(url):
    """Parse a svn URL, return the branch name and the path to it."""
    words = url.split('/')
    if 'tags' in words:
        start = words.index('tags')
        sep = '/'.join(words[start:start + 3])
    elif 'branches' in words:
        start = words.index('branches')
        sep = '/'.join(words[start:start + 2])
    else:
        assert 'trunk' in words
        sep = 'trunk'
    name = os.path.basename(sep)
    tagpath = url.split(sep, 1)[0] + sep
    return name, tagpath

def svn_info(arg):
    """Run 'svn info' and return the result as a list."""

    cmd = ['svn', 'info', arg]
    TRACE_LOGGER.debug(' '.join(cmd))
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    info = proc.communicate()[0]
    if proc.returncode:
        # Tolerate this error because a dev build might take place on
        # a host with an svn client incompatible with the checkout.
        return []
    return info.decode('utf-8').splitlines()

def scm_context(path, results=None):
    """Return the SCM dir (.git, .svn, ...) at the base of this checkout."""

    results = results or dict()
    path = os.path.abspath(path)

    for scm in ('.svn',):  # ignore .git to allow ad-hoc git use
        lookfor = os.path.join(path, scm)
        if os.path.isdir(lookfor):
            results['scm'] = results.get('scm', lookfor)

    marker = os.path.join(path, '.wlbase')
    if os.path.isfile(marker) and os.path.getsize(marker):
        results['marker'] = results.get('marker', marker)

    if len(results) == 2 or os.path.dirname(path) == path:
        # Either we have everything we need or we've reached the root.
        return results.get('scm'), results.get('marker')

    # Recurse.
    return scm_context(os.path.dirname(path), results=results)

def parse_args():
    """Get command line arguments."""

    class Formatter(argparse.ArgumentDefaultsHelpFormatter,
                    argparse.RawDescriptionHelpFormatter):
        """Print help defaults, do not format description."""

    parser = argparse.ArgumentParser(
        formatter_class=Formatter, description=__doc__)
    parser.add_argument(
        '--force', action='store_true',
        help='force header file update')
    parser.add_argument(
        '-k', '--key-comp-dir',
        '--workspace',  # deprecated alias
        help='workspace path to key component')
    parser.add_argument(
        '--key-comp-url',
        help='repo URL path to key component')
    parser.add_argument(
        '-v', '--verbosity', type=int,
        default=int(os.environ.get('EPIVERS_VERBOSITY', 1)),
        help='print more or less detailed output')
    parser.add_argument(  # TODO: remove this transitional flag
        '-V', '--old-verbosity', action='count', default=0,
        help=argparse.SUPPRESS)
    parser.add_argument(
        'header',
        help='path to generated header file')
    return parser.parse_args()

def main():
    """Conventional entry point for command line use."""

    # Option parsing.
    opts = parse_args()
    assert opts.key_comp_dir
    assert os.path.isdir(opts.key_comp_dir)
    header = opts.header
    key_url = opts.key_comp_url
    opts.verbosity += opts.old_verbosity
    pathstyle = os.path.abspath if opts.verbosity > 1 else os.path.relpath

    # Logging configuration.
    loglevel = logging.DEBUG if opts.verbosity > 1 else (
        logging.INFO if opts.verbosity else logging.ERROR)
    logging.basicConfig(format='%(filename)s: %(message)s',
                        level=loglevel,
                        stream=sys.stdout)
    trace_handler = logging.StreamHandler(sys.stdout)
    trace_handler.setLevel(loglevel)
    trace_handler.setFormatter(logging.Formatter('%(message)s'))
    TRACE_LOGGER.addHandler(trace_handler)
    TRACE_LOGGER.propagate = False

    tag, cmnt = None, ''
    if key_url:
        if '://' not in key_url:
            logging.warning('Warning: unexpanded URL keyword "%s"', key_url)
            key_url = None
        else:
            cmnt = 'key component URL'
            # If derived from an svn 'URL' keyword this may need its
            # prefix and suffix stripped.
            key_url = [p for p in key_url.split() if '://' in p][0]

    # Search for a local .svn/.git database or cached data in .wlbase.
    context, wlbase = scm_context(opts.key_comp_dir)

    if context and context.endswith('.git'):
        # git remains TBD
        assert False, 'unable to get SCM data from %s' % context

    epi_build_number = 0
    component_url = deps_url = rev = None

    # If a local context (a .svn dir) was found, query it for URL and revision.
    if context:
        cmnt += (' and ' if cmnt else '') + 'svn workspace "%s"' % (
            pathstyle(os.path.dirname(context)))
        for line in svn_info(os.path.dirname(context)):
            if line.startswith('URL:'):
                component_url = line.split()[-1]
            elif line.startswith(SVN_LCR):
                rev = line.split()[-1]

    if wlbase:
        # Fallback when no SCM metadata is present: use cached data from
        # the marker file. Warning: in a tag, even if SCM metadata is in
        # the checkout, we may still need this to derive deps_url!
        cmnt += (' and ' if cmnt else '') + os.path.basename(wlbase)
        with open(wlbase) as f:
            try:
                jdb = json.load(f)
            except ValueError as e:
                raise type(e)('%s within %s' % (e, os.path.abspath(wlbase)))

        # This key starts as the path from base to the key component dir.
        # It may have some prefix material e.g. "build/" and it may
        # also have a subdir within the component. We must first remove
        # the prefix and then work our way up till we reach the actual
        # component so "build/android/src/wl/sys" will become "src".
        key = os.path.relpath(opts.key_comp_dir, os.path.dirname(wlbase))
        while key:
            if key.startswith('src/') or key == 'src' or \
               key.startswith('components/'):
                break
            key = '/'.join(key.split('/')[1:])
        idb = jdb['SVN_INFO']
        cached_rev = cached_url = None
        while key:
            if key in idb:
                cached_rev = idb[key]['Last Changed Rev']
                cached_url = idb[key]['URL']
                break
            key = os.path.dirname(key)

        if cached_rev and not rev:
            logging.info('using cached rev from "%s"', pathstyle(wlbase))
            rev = cached_rev

        if cached_url and not component_url:
            logging.info('using cached URL from "%s"', pathstyle(wlbase))
            component_url = cached_url

        if 'DEPS' in jdb:
            deps_url = jdb['DEPS'][0]

    # Fallback when no data can be derived locally: if a URL was
    # provided we can use it for tag and query it for rev.
    if key_url and not rev:
        url = svn_urlparse(key_url)[1]
        reftime = os.getenv('SCM_REFTIME')
        if reftime:
            url = '@'.join([url, reftime])
        else:
            logging.warning('Warning: revision taken from "%s@HEAD"', url)

        rev = None
        for line in svn_info(url):
            if line.startswith(SVN_LCR):
                rev = line.split()[-1]
                break
        cmnt = 'svn repo %s revision' % url.split('@')[-1]

    assert rev, 'unable to derive revision from %s' % \
        os.path.abspath(opts.key_comp_dir)
    assert key_url or component_url or deps_url, 'unable to derive URL'

    # A URL is needed to derive tag and thus version numbering.
    # Give precedence to an explicitly passed URL, otherwise key off
    # the DEPS or component URLS. Currently DEPS gets priority which
    # is required for compatibility but they could be swapped.
    tag = svn_urlparse(key_url or deps_url or component_url)[0]

    if tag == 'trunk':
        # Can't derive branch/tag version data from ToT so use time instead.
        gmt = time.gmtime(NOW)
        epi_compound = time.strftime('%Y.%m.%d', gmt)
        epi_major_version = int(gmt.tm_year)
        epi_minor_version = int(gmt.tm_mon)
        epi_rc_number = int(gmt.tm_mday)
        epi_incremental_number = 0
        epi_version = '%d, %d, %d, 0' % (gmt.tm_year, gmt.tm_mon, gmt.tm_mday)
    else:
        # First two fields are assumed alphabetic, the rest numerical.
        fields = tag.split('_')[2:6]
        epi_compound = '.'.join(fields)
        if '_REL_' not in tag:
            epi_compound += ' (TOB)'
        # Pad with zeroes out to 4 numerical fields.
        while len(fields) <= 3:
            fields.append('0')
        epi_major_version = int(fields[0], 10)
        epi_minor_version = int(fields[1], 10)
        epi_rc_number = int(fields[2], 10)
        epi_incremental_number = int(fields[3], 10)
        epi_version = ', '.join(fields)

    epi_version_dev = '%d.%d.%d' % (
        epi_major_version,
        epi_minor_version,
        epi_rc_number)
    epi_version_num = '0x%02x%02x%02x%02x' % (
        epi_major_version,
        epi_minor_version,
        epi_rc_number,
        epi_incremental_number)
    # Truncate vernum to 32 bits. The string will keep full information.
    epi_version_num = epi_version_num[:10]

    scm_state = 'wlan=r%s' % rev

    # Generate text of header file from template ...
    contents = HEADER_TEMPLATE.lstrip().format(
        corp='Broadcom',
        epi_build_number=epi_build_number,
        epi_compound=epi_compound,
        epi_incremental_number=epi_incremental_number,
        epi_major_version=epi_major_version,
        epi_minor_version=epi_minor_version,
        epi_rc_number=epi_rc_number,
        epi_version=epi_version,
        epi_version_dev=epi_version_dev,
        epi_version_num=epi_version_num,
        scm_state=scm_state,
        year=time.gmtime(NOW).tm_year,
    )

    # If target file exists and is unchanged, skip it.
    if not opts.force:
        try:
            with open(header) as fi:
                if fi.read() == contents:
                    TRACE_LOGGER.debug(' '.join(sys.argv))
                    return
        except EnvironmentError:
            pass

    # Use tempfile+rename for atomic update (no race).
    with tempfile.NamedTemporaryFile(delete=False, dir=os.path.dirname(header),
                                     prefix=os.path.basename(header)) as f:
        f.write(contents.encode('utf-8'))
        tempname = f.name
    try:
        hdir = os.path.dirname(header) or '.'
        mode = (os.stat(hdir).st_mode ^ 0o111) & 0o000777
    except Exception as e:
        sys.stderr.write('%s\n' % e)
        mode = 0o644
    os.chmod(tempname, mode)
    os.rename(tempname, header)

    cmnt = 'for %s from %s: epi_compound="%s" scm_state="%s"' % (
        tag, cmnt, epi_compound, scm_state)

    TRACE_LOGGER.info(' '.join(sys.argv))
    logging.info('created %s %s', pathstyle(header), cmnt)

if __name__ == '__main__':
    main()

# vim: ts=8:sw=4:tw=80:et:

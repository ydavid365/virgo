#!/usr/bin/env python

import os
import subprocess
import sys
import signal

root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(root, 'tools'))
import paths


def test_cmd(additional=""):
    state_config = os.path.join(paths.root, 'contrib')
    monitoring_config = os.path.join(paths.root, 'agents', 'monitoring', 'tests', 'fixtures', 'monitoring-agent-localhost.cfg')
    zip_file = "monitoring.zip"
    cmd = '%s -o -z %s -c %s -s %s %s' % (paths.agent, zip_file, monitoring_config, state_config, additional)
    print cmd
    return cmd


def test_server_fixture(stdout=None):
    server = os.path.join('agents', 'monitoring', 'tests', 'fixtures', 'protocol', 'server.lua')

    cmd = "%s %s" % (paths.luvit, server)
    print cmd
    rc = 0
    if stdout is None:
        rc = subprocess.call(cmd, shell=True)
    else:
        rc = subprocess.call(cmd, shell=True, stdout=stdout)
    sys.exit(rc)


def test_server_fixture_blocking(stdout=None):
    server = os.path.join('agents', 'monitoring', 'tests', 'fixtures', 'protocol', 'server.lua')
    proc = subprocess.Popen([paths.luvit, server])

    def handler(signum, frame):
        proc.kill()

    signal.signal(signal.SIGUSR1, handler)
    rc = proc.wait()
    sys.exit(rc)


def test_endpoint(stdout=None):
    cmd = test_cmd()
    print cmd
    rc = 0
    rc = subprocess.call(cmd, shell=True, stdout=stdout)
    sys.exit(rc)


def test_endpoint_fixture(stdout=None):
    cmd = test_cmd("-i")
    print cmd
    rc = 0
    rc = subprocess.call(cmd, shell=True, stdout=stdout)
    sys.exit(rc)

commands = {
    'agent': test_endpoint,
    'agent_fixture': test_endpoint_fixture,
    'server_fixture': test_server_fixture,
    'server_fixture_blocking': test_server_fixture_blocking,
}


def usage():
    print('Usage: runner [%s]' % ', '.join(commands.keys()))
    sys.exit(1)

if len(sys.argv) != 2:
    usage()

ins = sys.argv[1]
if not ins in commands:
    print('Invalid command: %s' % ins)
    sys.exit(1)

print('Running %s' % ins)
cmd = commands.get(ins)
cmd()

#!/usr/bin/env python3
"""Compile exact production methods with the real class header, not a route reducer.

Extraction list below is explicit. Branches/statements are never filtered. The
consumer substitutes hardware, render dispatch, keyboard UI and persistence SDK
objects; it does not prove rendering, button scheduling or real radio behavior.
"""
import pathlib
import re
import sys

source = pathlib.Path(sys.argv[1]).read_text()
methods = [
    'WifiSelectionActivity', 'onEnter', 'onExit', 'startWifiScan', 'selectNetwork',
    'promptHiddenSsid', 'promptPasswordEntry', 'hasAttemptedAutoSsid',
    'tryAutoConnectCredential', 'tryNextSavedNetworkFromScan',
    'handleAutoConnectFailure', 'showNetworkListFromAutoConnect',
    'attemptConnection', 'checkConnectionStatus', 'completeConnection',
    'showDetails', 'onReferenceEvent', 'onComplete',
]
# Mask comments and literals without changing positions, then balance full bodies.
masked = re.sub(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                lambda m: ' ' * len(m.group()), source)
output = ['#include "consumer_environment.h"\n']
for name in methods:
    match = re.search(r'^.*\bWifiSelectionActivity::' + name + r'\(', masked, re.M)
    if not match:
        raise RuntimeError('Missing production method: ' + name)
    opening = masked.index('{', match.start())
    depth = 1
    end = opening + 1
    while depth:
        depth += (masked[end] == '{') - (masked[end] == '}')
        end += 1
    output.append(source[match.start():end] + '\n')
pathlib.Path(sys.argv[2]).write_text('\n'.join(output))

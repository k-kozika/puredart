#!/usr/bin/env python3
"""Record the demo's Frida invocation without injecting a process."""

import json
import sys

print(json.dumps(sys.argv[1:]))

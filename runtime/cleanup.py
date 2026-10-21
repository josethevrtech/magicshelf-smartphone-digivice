#!/usr/bin/env python3
"""Idempotent cleanup also runs after a crash through systemd ExecStopPost."""
import json,subprocess
# Game and Proton processes belong to this creation's systemd cgroup.

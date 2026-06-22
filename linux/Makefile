# Linux-only OpenA8DJ helper targets.

.PHONY: help status diagnostics self-test packages driver-status driver-module

LINUXCTL := tools/opena8dj-linuxctl
PACKAGE_BUILDER := packaging/build-experimental-packages.py

help:
	@printf '%s\n' 'OpenA8DJ Linux helper targets'
	@printf '%s\n' ''
	@printf '%s\n' 'Current state: diagnostic only, sound quality not validated'
	@printf '%s\n' ''
	@printf '%s\n' 'Targets:'
	@printf '%s\n' '  status        - run read-only Linux status'
	@printf '%s\n' '  diagnostics   - run read-only JSON diagnostics'
	@printf '%s\n' '  self-test     - run offline tool self-test'
	@printf '%s\n' '  packages      - build experimental .deb/.rpm artifacts'
	@printf '%s\n' '  driver-status - show driver scaffold status'
	@printf '%s\n' '  driver-module - intentionally refuses module build'

status:
	@$(LINUXCTL) status

diagnostics:
	@$(LINUXCTL) diagnostics --json

self-test:
	@$(LINUXCTL) self-test

packages:
	@$(PACKAGE_BUILDER)

driver-status:
	@$(MAKE) -C driver status

driver-module:
	@$(MAKE) -C driver module

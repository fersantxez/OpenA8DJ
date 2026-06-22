# Placeholder only. This is not a buildable OpenA8DJ Linux driver yet.

.PHONY: help status module install load unload

help:
	@printf '%s\n' 'OpenA8DJ Linux driver scaffold'
	@printf '%s\n' ''
	@printf '%s\n' 'Current state: diagnostic only, sound quality not validated'
	@printf '%s\n' ''
	@printf '%s\n' 'Available targets:'
	@printf '%s\n' '  help    - show this message'
	@printf '%s\n' '  status  - show scaffold status'
	@printf '%s\n' ''
	@printf '%s\n' 'Targets module/install/load/unload intentionally fail until'
	@printf '%s\n' 'the snd-usb-caiaq audit and kernel baseline are complete.'

status:
	@printf '%s\n' 'diagnostic only, sound quality not validated'
	@printf '%s\n' 'No buildable Linux kernel module is present in this scaffold.'

module install load unload:
	@printf '%s\n' 'Refusing: Linux driver implementation is not ready.'
	@printf '%s\n' 'Complete linux/SND_USB_CAIAQ_AUDIT.md against the chosen kernel first.'
	@false

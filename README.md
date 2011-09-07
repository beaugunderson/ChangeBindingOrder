ChangeBindingOrder
==================

ChangeBindingOrder allows you to move a network adapter to the top of the binding order.

This is useful when using OpenVPN, for example, because Windows will use the connected adapter with the highest order to resolve DNS entries. If you are connected to your home LAN and also connected via OpenVPN to your office and the LAN adapter has a higher order you may be unable to resolve DNS names on your office network. ChangeBindingOrder allows you to fix that issue automatically. It could be run via Group Policy or as a startup script, or as part of Microsoft's sysprep.

ChangeBindingOrder is currently hardcoded to move the OpenVPN TAP adapter but could be easily adapted to move another adapter (or to accept a command line argument).

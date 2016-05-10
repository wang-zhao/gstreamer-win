
#include "cairo-boilerplate-private.h"

void _cairo_boilerplate_register_all (void);


void _register_script();
void _register_win32_printing();
void _register_win32();
void _register_builtin();

void
_cairo_boilerplate_register_all (void)
{
	_register_script();
	_register_win32_printing();
	_register_win32();
	_register_builtin();
}
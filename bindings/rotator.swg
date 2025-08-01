/*
 *  Hamlib bindings - Rotator interface
 *  Copyright (c) 2001,2002 by Stephane Fillod
 *
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
%{
#include <hamlib/rotator.h>
%}

%include macros.swg
%immutable rot_caps::model_name;
%immutable rot_caps::mfg_name;
%immutable rot_caps::version;
%immutable rot_caps::copyright;

%include <hamlib/rotator.h>
%include <hamlib/rotlist.h>
%include <hamlib/rot_state.h>

%inline %{

typedef struct Rot {
	ROT *rot;
	struct rot_caps *caps;		/* shortcut to ROT->caps */
	struct rot_state *state;	/* shortcut to ROT->state */
	int error_status;
	int do_exception;
} Rot;

%}

/*
 * declare wrapper method with 0,1,2 arguments besides ROT*
 */
#define ROTMETHOD0(f) void f () \
				{ self->error_status = rot_##f(self->rot); }
#define ROTMETHOD1(f, t1) void f (t1 _##t1) \
				{ self->error_status = rot_##f(self->rot, _##t1); }
#define ROTMETHOD2(f, t1, t2) void f (t1 _##t1##_1, t2 _##t2##_2) \
				{ self->error_status = rot_##f(self->rot, _##t1##_1, _##t2##_2); }

%extend Rot {
	Rot(rot_model_t rot_model) {
		Rot *r;

		r = (Rot*)malloc(sizeof(Rot));
		if (!r)
			return NULL;
		r->rot = rot_init(rot_model);
		if (!r->rot) {
			free(r);
			return NULL;
		}
		/* install shortcuts */
		r->caps = r->rot->caps;
		r->state = &r->rot->state;
		r->do_exception = 0;    /* default is disabled */
		r->error_status = RIG_OK;
		return r;
	}
	~Rot () {
		rot_cleanup(self->rot);
		free(self);
	}

/*
 * return code checking
 */
%exception {
	arg1->error_status = RIG_OK;
	$action
	if (arg1->error_status != RIG_OK && arg1->do_exception)
		SWIG_exception(SWIG_UnknownError, rigerror(arg1->error_status));
}

	ROTMETHOD0(open)
	ROTMETHOD0(close)

	ROTMETHOD2(set_ext_func, hamlib_token_t, int)
	ROTMETHOD2(set_ext_level, hamlib_token_t, value_t)
	ROTMETHOD2(set_ext_parm, hamlib_token_t, value_t)

	ROTMETHOD2(set_func, setting_t, int)

	ROTMETHOD2(set_level, setting_t, value_t)

	ROTMETHOD2(set_parm, setting_t, value_t)

	ROTMETHOD2(set_position, azimuth_t, elevation_t)
        extern void get_position(azimuth_t *OUTPUT, elevation_t *OUTPUT);
	ROTMETHOD0(stop)
	ROTMETHOD0(park)
	ROTMETHOD1(reset, rot_reset_t)
	ROTMETHOD2(move, int, int)

	ROTMETHOD1(token_lookup, const_char_string)	/* conf */

	void set_conf(const char *name, const char *val) {
		hamlib_token_t tok = rot_token_lookup(self->rot, name);
		if (tok == RIG_CONF_END)
			self->error_status = -RIG_EINVAL;
		else
			self->error_status = rot_set_conf(self->rot, tok, val);
	}

	ROTMETHOD2(set_conf, hamlib_token_t, const_char_string)

#ifdef SWIGPYTHON
	ROT_GET_LONG(func)
	ROT_GET_LONG(ext_func)

	ROT_GET_VALUE_T(level)
	ROT_GET_VALUE_T(ext_level)

	ROT_GET_VALUE_T(parm)
	ROT_GET_VALUE_T(ext_parm)
#endif

        const char *get_conf(hamlib_token_t tok) {
                static char s[128] = "";
                self->error_status = rot_get_conf(self->rot, tok, s);
                return s;
        }

        const char *get_conf(const char *name) {
                hamlib_token_t tok = rot_token_lookup(self->rot, name);
                static char s[128] = "";
                if (tok == RIG_CONF_END)
                        self->error_status = -RIG_EINVAL;
                else
                        self->error_status = rot_get_conf(self->rot, tok, s);
                return s;
        }

        const char * get_info(void) {
                const char *s;
                s = rot_get_info(self->rot);
                self->error_status = s ? RIG_OK : -RIG_EINVAL;
                return s;
        }

	/* TODO: get_conf_list, .. */
};

%{

/*
 * this one returns 2 values, here is a perl example:
 *      ($az, $elevation) = $rig->get_position();
 */
void Rot_get_position(Rot *self, azimuth_t *azimuth, elevation_t *elevation)
{
        self->error_status = rot_get_position(self->rot, azimuth, elevation);
}

%}

/* marshal.c
 *
 * Copyright 2026 Eva M
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "marshal.h"

void
_marshal_DIRECT__ARGS_DIRECT (GClosure                *closure,
                              GValue                  *return_value,
                              guint                    n_param_values,
                              const GValue            *param_values,
                              gpointer invocation_hint G_GNUC_UNUSED,
                              gpointer                 marshal_data)
{
  typedef void (*GMarshalFunc_DIRECT__ARGS_DIRECT) (gpointer      data1,
                                                    GValue       *return_value,
                                                    guint         n_param_values,
                                                    const GValue *param_values,
                                                    gpointer      data2);
  GCClosure                       *cc = (GCClosure *) closure;
  gpointer                         data1, data2;
  GMarshalFunc_DIRECT__ARGS_DIRECT callback;

  g_return_if_fail (return_value != NULL);
  g_return_if_fail (n_param_values >= 1);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_DIRECT__ARGS_DIRECT) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            return_value,
            n_param_values - 1,
            param_values + 1,
            data2);
}

void
_marshal_BOXED__ARGS_DIRECT (GClosure                *closure,
                             GValue                  *return_value,
                             guint                    n_param_values,
                             const GValue            *param_values,
                             gpointer invocation_hint G_GNUC_UNUSED,
                             gpointer                 marshal_data)
{
  typedef gpointer (*GMarshalFunc_BOXED__ARGS_DIRECT) (gpointer      data1,
                                                       guint         n_param_values,
                                                       const GValue *param_values,
                                                       gpointer      data2);
  GCClosure                      *cc = (GCClosure *) closure;
  gpointer                        data1, data2;
  GMarshalFunc_BOXED__ARGS_DIRECT callback;
  gpointer                        v_return;

  g_return_if_fail (return_value != NULL);
  g_return_if_fail (n_param_values >= 1);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_BOXED__ARGS_DIRECT) (marshal_data ? marshal_data : cc->callback);

  v_return = callback (data1,
                       n_param_values - 1,
                       param_values + 1,
                       data2);

  g_value_take_boxed (return_value, v_return);
}

void
_marshal_BOOL__ARGS_DIRECT (GClosure                *closure,
                            GValue                  *return_value,
                            guint                    n_param_values,
                            const GValue            *param_values,
                            gpointer invocation_hint G_GNUC_UNUSED,
                            gpointer                 marshal_data)
{
  typedef gboolean (*GMarshalFunc_BOOL__ARGS_DIRECT) (gpointer      data1,
                                                      guint         n_param_values,
                                                      const GValue *param_values,
                                                      gpointer      data2);
  GCClosure                     *cc = (GCClosure *) closure;
  gpointer                       data1, data2;
  GMarshalFunc_BOOL__ARGS_DIRECT callback;
  gboolean                       v_return;

  g_return_if_fail (return_value != NULL);
  g_return_if_fail (n_param_values >= 1);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_BOOL__ARGS_DIRECT) (marshal_data ? marshal_data : cc->callback);

  v_return = callback (data1,
                       n_param_values - 1,
                       param_values + 1,
                       data2);

  g_value_set_boolean (return_value, v_return);
}

void
_marshal_DOUBLE__ARGS_DIRECT (GClosure                *closure,
                              GValue                  *return_value,
                              guint                    n_param_values,
                              const GValue            *param_values,
                              gpointer invocation_hint G_GNUC_UNUSED,
                              gpointer                 marshal_data)
{
  typedef gdouble (*GMarshalFunc_DOUBLE__ARGS_DIRECT) (gpointer      data1,
                                                       guint         n_param_values,
                                                       const GValue *param_values,
                                                       gpointer      data2);
  GCClosure                       *cc = (GCClosure *) closure;
  gpointer                         data1, data2;
  GMarshalFunc_DOUBLE__ARGS_DIRECT callback;
  gdouble                          v_return;

  g_return_if_fail (return_value != NULL);
  g_return_if_fail (n_param_values >= 1);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_DOUBLE__ARGS_DIRECT) (marshal_data ? marshal_data : cc->callback);

  v_return = callback (data1,
                       n_param_values - 1,
                       param_values + 1,
                       data2);

  g_value_set_double (return_value, v_return);
}

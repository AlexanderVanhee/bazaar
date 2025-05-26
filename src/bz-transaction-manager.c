/* bz-transaction-manager.c
 *
 * Copyright 2025 Adam Masciola
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

#include "config.h"

#include <glib/gi18n.h>

#include "bz-transaction-manager.h"
#include "bz-transaction-view.h"
#include "bz-util.h"

struct _BzTransactionManager
{
  AdwBin parent_instance;

  BzBackend     *backend;
  GListStore    *transactions;
  double         current_progress;
  BzTransaction *last_success;

  DexFuture *current_task;
  GQueue     queue;

  /* Template widgets */
  GtkListView    *list_view;
  GtkNoSelection *no_selection;
};

G_DEFINE_FINAL_TYPE (BzTransactionManager, bz_transaction_manager, ADW_TYPE_BIN)

enum
{
  PROP_0,

  PROP_BACKEND,
  PROP_HAS_TRANSACTIONS,
  PROP_ACTIVE,
  PROP_CURRENT_PROGRESS,
  PROP_LAST_SUCCESS,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

BZ_DEFINE_DATA (
    queued_schedule,
    QueuedSchedule,
    {
      BzTransactionManager *self;
      GPtrArray            *transactions;
      GHashTable           *entry_to_transaction_hash;
    },
    BZ_RELEASE_DATA (self, g_object_unref);
    BZ_RELEASE_DATA (transactions, g_ptr_array_unref);
    BZ_RELEASE_DATA (entry_to_transaction_hash, g_hash_table_unref));

static void
transaction_progress (BzEntry            *entry,
                      const char         *status,
                      gboolean            is_estimating,
                      int                 progress,
                      guint64             bytes_transferred,
                      guint64             start_time,
                      QueuedScheduleData *data);

static DexFuture *
transaction_finally (DexFuture          *future,
                     QueuedScheduleData *data);

static void
dispatch_next (BzTransactionManager *self);

static void
bz_transaction_manager_dispose (GObject *object)
{
  BzTransactionManager *self = BZ_TRANSACTION_MANAGER (object);

  g_clear_object (&self->backend);
  g_clear_object (&self->transactions);
  g_clear_object (&self->last_success);
  g_queue_clear_full (&self->queue, queued_schedule_data_unref);
  dex_clear (&self->current_task);

  G_OBJECT_CLASS (bz_transaction_manager_parent_class)->dispose (object);
}

static void
bz_transaction_manager_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  BzTransactionManager *self = BZ_TRANSACTION_MANAGER (object);

  switch (prop_id)
    {
    case PROP_BACKEND:
      g_value_set_object (value, bz_transaction_manager_get_backend (self));
      break;
    case PROP_HAS_TRANSACTIONS:
      g_value_set_boolean (value, g_list_model_get_n_items (G_LIST_MODEL (self->transactions)) > 0);
      break;
    case PROP_ACTIVE:
      g_value_set_boolean (value, self->current_task != NULL);
      break;
    case PROP_CURRENT_PROGRESS:
      g_value_set_double (value, self->current_progress);
      break;
    case PROP_LAST_SUCCESS:
      g_value_set_object (value, self->last_success);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_transaction_manager_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  BzTransactionManager *self = BZ_TRANSACTION_MANAGER (object);

  switch (prop_id)
    {
    case PROP_BACKEND:
      bz_transaction_manager_set_backend (self, g_value_get_object (value));
      break;
    case PROP_HAS_TRANSACTIONS:
    case PROP_ACTIVE:
    case PROP_CURRENT_PROGRESS:
    case PROP_LAST_SUCCESS:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static gboolean
invert_boolean (gpointer object,
                gboolean value)
{
  return !value;
}

static void
bz_transaction_manager_class_init (BzTransactionManagerClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_transaction_manager_dispose;
  object_class->get_property = bz_transaction_manager_get_property;
  object_class->set_property = bz_transaction_manager_set_property;

  props[PROP_BACKEND] =
      g_param_spec_object (
          "backend",
          NULL, NULL,
          BZ_TYPE_BACKEND,
          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_HAS_TRANSACTIONS] =
      g_param_spec_boolean (
          "has-transactions",
          NULL, NULL,
          FALSE,
          G_PARAM_READABLE);

  props[PROP_ACTIVE] =
      g_param_spec_boolean (
          "active",
          NULL, NULL,
          FALSE,
          G_PARAM_READABLE);

  props[PROP_CURRENT_PROGRESS] =
      g_param_spec_double (
          "current-progress",
          NULL, NULL,
          0.0, 1.0, 0.0,
          G_PARAM_READABLE);

  props[PROP_LAST_SUCCESS] =
      g_param_spec_object (
          "last-success",
          NULL, NULL,
          BZ_TYPE_TRANSACTION,
          G_PARAM_READABLE);

  g_type_ensure (BZ_TYPE_TRANSACTION_VIEW);
  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/bazaar/bz-transaction-manager.ui");
  gtk_widget_class_bind_template_child (widget_class, BzTransactionManager, list_view);
  gtk_widget_class_bind_template_child (widget_class, BzTransactionManager, no_selection);
  gtk_widget_class_bind_template_callback (widget_class, invert_boolean);
}

static void
bz_transaction_manager_init (BzTransactionManager *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->transactions = g_list_store_new (BZ_TYPE_TRANSACTION);
  g_queue_init (&self->queue);

  gtk_no_selection_set_model (self->no_selection, G_LIST_MODEL (self->transactions));
}

GtkWidget *
bz_transaction_manager_new (GListModel *model)
{
  return g_object_new (
      BZ_TYPE_TRANSACTION_MANAGER,
      "model", model,
      NULL);
}

void
bz_transaction_manager_set_backend (BzTransactionManager *self,
                                    BzBackend            *backend)
{
  g_return_if_fail (BZ_IS_TRANSACTION_MANAGER (self));
  g_return_if_fail (backend == NULL || BZ_IS_BACKEND (backend));

  g_clear_object (&self->backend);
  if (backend != NULL)
    self->backend = g_object_ref (backend);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_BACKEND]);
}

BzBackend *
bz_transaction_manager_get_backend (BzTransactionManager *self)
{
  g_return_val_if_fail (BZ_IS_TRANSACTION_MANAGER (self), NULL);
  return self->backend;
}

BzTransaction *
bz_transaction_manager_get_last_success (BzTransactionManager *self)
{
  g_return_val_if_fail (BZ_IS_TRANSACTION_MANAGER (self), NULL);
  return self->last_success;
}

void
bz_transaction_manager_add (BzTransactionManager *self,
                            BzTransaction        *transaction)
{
  /* TODO: be smarter and merge transactions based on stuff */
  g_autoptr (QueuedScheduleData) data = NULL;
  GListModel *installs                = NULL;
  GListModel *updates                 = NULL;
  GListModel *removals                = NULL;
  guint       n_installs              = 0;
  guint       n_updates               = 0;
  guint       n_removals              = 0;

  g_return_if_fail (BZ_IS_TRANSACTION_MANAGER (self));
  g_return_if_fail (self->backend != NULL);
  g_return_if_fail (BZ_IS_TRANSACTION (transaction));

  data                            = queued_schedule_data_new ();
  data->self                      = NULL;
  data->transactions              = g_ptr_array_new_with_free_func (g_object_unref);
  data->entry_to_transaction_hash = g_hash_table_new_full (
      g_direct_hash, g_direct_equal, g_object_unref, g_object_unref);

  g_ptr_array_add (data->transactions, g_object_ref (transaction));

  installs = bz_transaction_get_installs (transaction);
  updates  = bz_transaction_get_updates (transaction);
  removals = bz_transaction_get_removals (transaction);

  if (installs != NULL)
    n_installs = g_list_model_get_n_items (installs);
  if (updates != NULL)
    n_updates = g_list_model_get_n_items (updates);
  if (removals != NULL)
    n_removals = g_list_model_get_n_items (removals);

  for (guint i = 0; i < n_installs; i++)
    g_hash_table_replace (data->entry_to_transaction_hash,
                          g_list_model_get_item (installs, i),
                          g_object_ref (transaction));
  for (guint i = 0; i < n_updates; i++)
    g_hash_table_replace (data->entry_to_transaction_hash,
                          g_list_model_get_item (updates, i),
                          g_object_ref (transaction));
  for (guint i = 0; i < n_removals; i++)
    g_hash_table_replace (data->entry_to_transaction_hash,
                          g_list_model_get_item (removals, i),
                          g_object_ref (transaction));

  g_queue_push_head (&self->queue, queued_schedule_data_ref (data));
  if (self->current_task == NULL)
    dispatch_next (self);

  g_list_store_insert (self->transactions, 0, transaction);
  gtk_list_view_scroll_to (self->list_view, 0, GTK_LIST_SCROLL_FOCUS, NULL);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_HAS_TRANSACTIONS]);
}

static void
transaction_progress (BzEntry            *entry,
                      const char         *status,
                      gboolean            is_estimating,
                      int                 progress,
                      guint64             bytes_transferred,
                      guint64             start_time,
                      QueuedScheduleData *data)
{
  BzTransaction *transaction     = NULL;
  double         progress_double = 0.0;

  transaction = g_hash_table_lookup (data->entry_to_transaction_hash, entry);
  g_assert (transaction != NULL);

  progress_double = (double) progress / 100.0;
  g_object_set (
      transaction,
      "pending", is_estimating,
      "status", status,
      "progress", progress_double,
      NULL);

  data->self->current_progress = progress_double;
  g_object_notify_by_pspec (G_OBJECT (data->self), props[PROP_CURRENT_PROGRESS]);
}

static DexFuture *
transaction_finally (DexFuture          *future,
                     QueuedScheduleData *data)
{
  g_autoptr (GError) local_error = NULL;
  const GValue *value            = NULL;

  value = dex_future_get_value (future, &local_error);

  for (guint i = 0; i < data->transactions->len; i++)
    {
      BzTransaction *transaction = NULL;

      transaction = g_ptr_array_index (data->transactions, i);
      g_object_set (
          transaction,
          "status", _ ("Finished!"),
          "progress", 1.0,
          "finished", TRUE,
          "success", value != NULL,
          "error", local_error != NULL ? local_error->message : NULL,
          NULL);

      if (value != NULL)
        {
          g_clear_object (&data->self->last_success);
          data->self->last_success = g_object_ref (transaction);

          g_object_notify_by_pspec (
              G_OBJECT (data->self),
              props[PROP_LAST_SUCCESS]);
        }
    }

  dispatch_next (data->self);
  return NULL;
}

static void
dispatch_next (BzTransactionManager *self)
{
  g_autoptr (QueuedScheduleData) data = NULL;
  g_autoptr (GListStore) store        = NULL;
  DexFuture *future                   = NULL;

  dex_clear (&self->current_task);

  if (self->queue.length == 0)
    goto done;
  data       = g_queue_pop_tail (&self->queue);
  data->self = g_object_ref (self);

  store = g_list_store_new (BZ_TYPE_TRANSACTION);
  for (guint i = 0; i < data->transactions->len; i++)
    {
      BzTransaction *transaction = NULL;

      transaction = g_ptr_array_index (data->transactions, i);
      g_list_store_append (store, transaction);
    }

  future = bz_backend_merge_and_schedule_transactions (
      self->backend,
      G_LIST_MODEL (store),
      (BzBackendTransactionProgressFunc) transaction_progress,
      queued_schedule_data_ref (data),
      queued_schedule_data_unref);
  future = dex_future_finally (
      future, (DexFutureCallback) transaction_finally,
      queued_schedule_data_ref (data),
      queued_schedule_data_unref);
  self->current_task = future;

done:
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ACTIVE]);
}

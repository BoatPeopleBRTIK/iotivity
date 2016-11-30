/******************************************************************
 *
 * Copyright 2015 Intel Corporation All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************/

#include "client.h"
#include "recv.h"
#include "context.h"
#include "bluez.h"
#include "utils.h"

#include "cagattservice.h"
#include "logger.h"
#include "oic_malloc.h"
#include "oic_string.h"

#include <gio/gio.h>

#include <strings.h>
#include <assert.h>


// Logging tag.
static char const TAG[] = "BLE_CLIENT";

typedef struct _CAGattClientContext
{
    /**
     * Bluetooth MAC address to GATT characteristic map.
     *
     * Hash table that maps Bluetooth MAC address to a OIC Transport
     * Profile GATT characteristic.  The key is a string containing
     * the LE peripheral Bluetooth adapter MAC address.   The value is
     * an interface proxy (@c GDBusProxy) to an
     * @c org.bluez.GattCharacteristic1 object, i.e. OIC Transport
     * Profile GATT request characteristic.
     */
    GHashTable * characteristic_map;

    /**
     * Response characteristic object path to Bluetooth MAC address map.
     *
     * Hash table that maps OIC Transport Profile GATT response
     * characteristic D-Bus object path to Bluetooth MAC address.  The
     * key is the D-Bus object path.  The value is the LE peripheral
     * Bluetooth adapter MAC address.
     *
     * @note This map exists to avoid having to create a hierarchy of
     *       GLib D-Bus proxies to the client side BlueZ GATT response
     *       Characteristic, its corresponding GATT Service, and the
     *       Device object within that Service, along with the
     *       resulting D-Bus calls, simply so that we obtain the MAC
     *       address of the remote (peripheral) LE device.  We
     *       unfortunately need the MAC address since the
     *       shared/common caleadapter code requires it.
     */
    GHashTable * address_map;

    /// Mutex used to synchronize access to context fields.
    ca_mutex lock;

} CAGattClientContext;

static CAGattClientContext g_context = {
    .lock = NULL
};

// ---------------------------------------------------------------------
//                      GATT Response Receive
// ---------------------------------------------------------------------
static void CAGattClientOnCharacteristicPropertiesChanged(
    GDBusProxy * characteristic,
    GVariant * changed_properties,
    GStrv invalidated_properties,
    gpointer user_data)
{
    /*
      This handler is trigged in a GATT client when receiving data
      sent by a GATT server through a notification, e.g. such as when
      a GATT server sent a response.
    */

    (void) invalidated_properties;

    if (g_variant_n_children(changed_properties) < 1)
    {
        /*
          No changed properties, only invalidated ones which we don't
          care about.
        */
        return;
    }

    CALEContext * const context = user_data;
    char const * const object_path =
        g_dbus_proxy_get_object_path(characteristic);

    ca_mutex_lock(g_context.lock);

    char * const address =
        g_hash_table_lookup(g_context.address_map, object_path);

    /*
      Address lookup could fail if a property changed on a GATT
      characteristic that isn't an OIC Transport Profile GATT response
      characteristic.  This isn't necessarily a problem since it's
      possible other unrelated GATT charactertistics with changed
      properties are exposed by BlueZ on the D-Bus system bus.
    */
    if (address != NULL)
    {
        CAGattRecvInfo info =
            {
                .peer               = address,
                .on_packet_received = context->on_client_received_data,
                .context            = context
            };

        GVariant * const value =
            g_variant_lookup_value(changed_properties, "Value", NULL);

        if (value != NULL)
        {
            // GLib maps an octet to a guchar, which is of size 1.
            gsize length = 0;
            gconstpointer const data =
                g_variant_get_fixed_array(value, &length, 1);

            (void) CAGattRecv(&info, data, length);

            g_variant_unref(value);
        }
    }

    ca_mutex_unlock(g_context.lock);
}

// ---------------------------------------------------------------------
//                        GATT Client Set-up
// ---------------------------------------------------------------------
static bool CAGattClientMapInsert(GHashTable * map,
                                  gpointer key,
                                  gpointer value)
{
    bool const insert = !g_hash_table_contains(map, key);

    if (insert)
    {
        g_hash_table_insert(map, key, value);
    }

    return insert;
}

static bool CAGattGetClientObjectProxies(GList ** proxies,
                                         char const * interface,
                                         GList * objects,
                                         char const * object_path,
                                         char const * property)
{
    if (interface == NULL
            || proxies == NULL
            || objects == NULL
            || object_path == NULL
            || property == NULL)
    {
        OIC_LOG(ERROR, TAG, "Invalid input parameters");
        return false;
    }

    bool success = true;
    GList * l = NULL;

    for ( l = objects; l != NULL; l = l->next)
    {
        GDBusProxy * const proxy =
            G_DBUS_PROXY(g_dbus_object_get_interface(
                G_DBUS_OBJECT(l->data),
                interface));
        if (proxy != NULL)
        {
            GVariant * const proxy_property =
                g_dbus_proxy_get_cached_property(proxy, property);

            if (proxy_property != NULL)
            {
                char *proxy_object = NULL;
                g_variant_get(proxy_property, "o", &proxy_object);
                g_variant_unref(proxy_property);

                if (proxy_object != NULL && strcmp(proxy_object, object_path) == 0)
                {
                    *proxies = g_list_prepend(*proxies, proxy);
                }
                else
                {
                    g_object_unref(proxy);
                }
            }
            else
            {
                g_object_unref(proxy);
            }
        }
    }
    return success;
}

static bool CAGattClientSetupCharacteristics(
    GDBusProxy * service,
    char const * address,
    GHashTable * characteristic_map,
    GHashTable * address_map,
    CALEContext * context)
{
    bool success = true;
    char const * const service_object_path =
        g_dbus_proxy_get_object_path(service);
    GList * characteristics = NULL;
    GList * l = NULL;
    GList * local_objects = g_dbus_object_manager_get_objects(context->object_manager);

    CAGattGetClientObjectProxies(&characteristics,
                                 BLUEZ_GATT_CHARACTERISTIC_INTERFACE,
                                 local_objects,
                                 service_object_path,
                                 "Service");
    if (characteristics == NULL)
    {
        OIC_LOG(ERROR, TAG, "org.bluez.GattCharacteristic1 not found");
        success = false;
        return success;
    }
    for (l = characteristics; l != NULL && success; l = l->next)
    {
        GDBusProxy * const characteristic = G_DBUS_PROXY(l->data);
        GVariant * const uuid_prop =
            g_dbus_proxy_get_cached_property(characteristic, "UUID");

        char const * const uuid =
            g_variant_get_string(uuid_prop, NULL);

        if (strcasecmp(uuid, CA_GATT_REQUEST_CHRC_UUID) == 0)
        {
            char     * const addr = OICStrdup(address);
            gpointer * const chrc = g_object_ref(characteristic);

            // Map LE (MAC) address to request characteristic.
            if (!CAGattClientMapInsert(characteristic_map, addr, chrc))
            {
                OIC_LOG_V(WARNING,
                          TAG,
                          "Possible duplicate OIC GATT "
                          "request characteristic proxy detected.");

                g_object_unref(chrc);
                OICFree(addr);
            }
        }
        else if (strcasecmp(uuid, CA_GATT_RESPONSE_CHRC_UUID) == 0)
        {
            char * const p =
                OICStrdup(g_dbus_proxy_get_object_path(characteristic));
            char * const addr = OICStrdup(address);

            // Map GATT service D-Bus object path to client address.
            if (!CAGattClientMapInsert(address_map, p, addr))
            {
                OIC_LOG_V(WARNING,
                          TAG,
                          "Unable to register duplicate "
                          "peripheral MAC address");

                success = false;

                OICFree(addr);
                OICFree(p);
            }
            else
            {
                /*
                  Detect changes in GATT characteristic properties.
                  This is only relevant to OIC response
                  characteristics since only their "Value" property
                  will ever change.
                */
                g_signal_connect(
                    characteristic,
                    "g-properties-changed",
                    G_CALLBACK(CAGattClientOnCharacteristicPropertiesChanged),
                    context);

                GError * error = NULL;

                // Enable notifications.
                GVariant * const ret =
                    g_dbus_proxy_call_sync(
                        characteristic,
                        "StartNotify",
                        NULL,  // parameters
                        G_DBUS_CALL_FLAGS_NONE,
                        -1,    // timeout (default == -1),
                        NULL,  // cancellable
                        &error);

                if (ret == NULL)
                {
                    OIC_LOG_V(ERROR,
                              TAG,
                              "Failed to enable GATT notifications: %s",
                              error->message);

                    g_error_free(error);
                    g_hash_table_remove(address_map, address);
                    success = false;
                }
                else
                {
                    g_variant_unref(ret);
                }
            }
        }
#ifdef TB_LOG
        else
        {
            OIC_LOG_V(WARNING,
                      TAG,
                      "Unrecognized characteristic UUID "
                      "in OIC GATT service: %s",
                      uuid);
        }
#endif

        g_variant_unref(uuid_prop);
    }

    // Free characteristics.
    g_list_free_full(characteristics, g_object_unref);
    g_list_free_full(local_objects, g_object_unref);

    return success;
}

static bool CAGattClientSetupService(
    GDBusProxy * device,
    GHashTable * characteristic_map,
    GHashTable * address_map,
    CALEContext * context)
{
    bool success = true;

    GVariant * const address_prop =
        g_dbus_proxy_get_cached_property(device, "Address");

    char const * const address =
        g_variant_get_string(address_prop, NULL);

    char const * const device_object_path =
            g_dbus_proxy_get_object_path(device);

    /*
      Create a proxies to the org.bluez.GattService1 D-Bus objects
      that implement the OIC Transport Profile on the client side.

      The services_prop argument will be non-NULL if changes to the
      org.bluez.Device1.GattServices property were detected
      asynchronously through the PropertiesChanged signal.
    */
    GVariant *service_resolved_pro =
            g_dbus_proxy_get_cached_property(device, "ServicesResolved");
    bool is_service_resolved = g_variant_get_boolean(service_resolved_pro);

    g_variant_unref(service_resolved_pro);

    if (!is_service_resolved)
    {
        return false;
    }
    GList * services = NULL;
    GList * l = NULL;
    GList * local_objects = g_dbus_object_manager_get_objects(context->object_manager);
    CAGattGetClientObjectProxies(&services,
                                 BLUEZ_GATT_SERVICE_INTERFACE,
                                 local_objects,
                                 device_object_path,
                                 "Device");
    if (services == NULL)
    {
        OIC_LOG(ERROR, TAG,
                    "Unable to obtain proxy to GATT service");
        return false;
    }

    for (l = services; l != NULL && success; l = l->next)
    {
        GDBusProxy * const service = G_DBUS_PROXY(l->data);

        GVariant * const uuid_prop =
            g_dbus_proxy_get_cached_property(service, "UUID");

        char const * const uuid =
            g_variant_get_string(uuid_prop, NULL);

        if (strcasecmp(uuid, CA_GATT_SERVICE_UUID) == 0)
        {
            success = CAGattClientSetupCharacteristics(service,
                                                       address,
                                                       characteristic_map,
                                                       address_map,
                                                       context);

#ifdef TB_LOG
            if (!success)
            {
                OIC_LOG_V(ERROR,
                          TAG,
                          "Characteristic set up for "
                          "GATT service at %s failed.",
                          address);
            }
#endif  // TB_LOG
        }

        g_variant_unref(uuid_prop);
    }

    g_variant_unref(address_prop);
    // Free services.
    g_list_free_full(services, g_object_unref);
    g_list_free_full(local_objects, g_object_unref);
    return success;
}

static void CAGattClientRemoveCharacteristics(
    GDBusProxy * device,
    GHashTable * characteristic_map)
{
    GVariant * const address_prop =
            g_dbus_proxy_get_cached_property(device, "Address");
    char const * const address =
            g_variant_get_string(address_prop, NULL);
    g_variant_unref(address_prop);

    ca_mutex_lock(g_context.lock);

    if (characteristic_map)
    {
        if (g_hash_table_contains(characteristic_map,address))
        {
            g_hash_table_remove (characteristic_map,address);
        }
    }

    ca_mutex_unlock(g_context.lock);

}

void CAGattClientRemoveAddress(char const * object_path)
{
    if (g_context.address_map)
    {
        if (g_hash_table_contains(g_context.address_map,object_path))
        {
            g_hash_table_remove (g_context.address_map,object_path);
        }
    }
}

static void CAGattClientOnDevicePropertiesChanged(
    GDBusProxy * device,
    GVariant * changed_properties,
    GStrv invalidated_properties,
    gpointer user_data)
{
    /*
      This handler is trigged in a GATT client when org.bluez.Device1
      properties have changed.
    */

    (void) invalidated_properties;

    /*
      Retrieve the org.bluez.Device1.ServicesResolved property from the
      changed_properties dictionary parameter (index 1).
    */
    GVariant *service_resolved_pro =
            g_variant_lookup_value(changed_properties, "ServicesResolved", NULL);

    GVariant * const address_prop =
            g_dbus_proxy_get_cached_property(device, "Address");
    char const * const address =
            g_variant_get_string(address_prop, NULL);
    g_variant_unref(address_prop);

    if (service_resolved_pro)
    {
        bool is_service_resolved = g_variant_get_boolean(service_resolved_pro);
        g_variant_unref(service_resolved_pro);

        if (is_service_resolved)
        {
            CALEContext * const context = user_data;
            ca_mutex_lock(g_context.lock);

            bool resulte = CAGattClientSetupService(device,
                                     g_context.characteristic_map,
                                     g_context.address_map,
                                     context);
            ca_mutex_unlock(g_context.lock);
            if (!resulte)
            {
                CAGattClientRemoveCharacteristics(device,
                                              g_context.characteristic_map);

                g_dbus_proxy_call(device,
                                  "Disconnect",
                                  NULL,  // parameters
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1,    // timeout (default == -1),
                                  NULL,  // cancellable
                                  NULL,  // callback
                                  NULL); // user data

                CAAddBlackList(address);
            }
        }
        else
        {
            CAGattClientRemoveCharacteristics(device,
                                              g_context.characteristic_map);
        }
    }

    GVariant *connect_prop =
            g_variant_lookup_value(changed_properties, "Connected", NULL);
    if (connect_prop)
    {
        bool is_connected = g_variant_get_boolean(connect_prop);
        g_variant_unref(connect_prop);

        if (!is_connected)
        {
            CALEContext * const context = (CALEContext *) user_data;
            GList * l = NULL;

            ca_mutex_lock(context->lock);

            for (l = context->devices; l != NULL; l = g_list_next(l))
            {
                GDBusProxy * const proxy = G_DBUS_PROXY(l->data);

                if (strcmp(g_dbus_proxy_get_object_path(device),
                           g_dbus_proxy_get_object_path(proxy)) == 0)
                {
                    g_object_unref(proxy);
                    context->devices = g_list_delete_link(context->devices, l);
                    break;
                }
            }
            CARemoveDevice(device, context);

            ca_mutex_unlock(context->lock);

            if (context->on_connection_state_changed)
            {
                context->on_connection_state_changed(CA_ADAPTER_GATT_BTLE,
                                                     address,
                                                     false);
            }
        }
    }
}

CAResult_t CAGattClientInitialize()
{
    if (!g_context.lock){
        g_context.lock = ca_mutex_new();

        /*
          Map Bluetooth MAC address to OIC Transport Profile
          request characteristics.
        */
        GHashTable * const characteristic_map =
            g_hash_table_new_full(g_str_hash,
                                  g_str_equal,
                                  OICFree,
                                  g_object_unref);

        /*
          Map OIC Transport Profile response characteristic D-Bus object
          path to Bluetooth MAC address.
        */
        GHashTable * const address_map =
            g_hash_table_new_full(g_str_hash,
                                  g_str_equal,
                                  OICFree,
                                  OICFree);

        ca_mutex_lock(g_context.lock);

        g_context.characteristic_map = characteristic_map;
        g_context.address_map = address_map;

        ca_mutex_unlock(g_context.lock);
    }

    return CA_STATUS_OK;
}

bool CAGattClientConnected(CALEContext * context, GDBusProxy * const device)
{
    bool result = false;

    /*
      Detect changes in BlueZ Device properties.  This is
      predominantly used to detect GATT services that were
      discovered asynchronously.
    */
    g_signal_connect(
        device,
        "g-properties-changed",
        G_CALLBACK(CAGattClientOnDevicePropertiesChanged),
        context);
    return CAGattClientSetupService(device,
                             g_context.characteristic_map,
                             g_context.address_map,
                             context);
}

void CAGattClientDestroy()
{
    if (g_context.lock == NULL)
    {
        return;  // Initialization did not complete.
    }

    ca_mutex_lock(g_context.lock);

    if (g_context.characteristic_map != NULL)
    {
        g_hash_table_unref(g_context.characteristic_map);
        g_context.characteristic_map = NULL;
    }

    if (g_context.address_map != NULL)
    {
        g_hash_table_unref(g_context.address_map);
        g_context.address_map = NULL;
    }

    ca_mutex_unlock(g_context.lock);

    ca_mutex_free(g_context.lock);
    g_context.lock = NULL;

    /*
      We don't explicitly stop notifications on the response
      characteristic since they should be stopped upon server side
      disconnection.
     */
}

// ---------------------------------------------------------------------
//                      GATT Request Data Send
// ---------------------------------------------------------------------

static CAResult_t CAGattClientSendDataImpl(GDBusProxy * characteristic,
                                           uint8_t const * data,
                                           size_t length,
                                           CALEContext * context)
{
    assert(characteristic != NULL);
    assert(data != NULL);
    assert(context != NULL);

    GVariant * const value =
        g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,
                                  data,
                                  length,
                                  1);  // sizeof(data[0]) == 1
    GVariantBuilder *builder = NULL;

    builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));

    g_variant_builder_add(builder, "{sv}", "offset", g_variant_new_uint16(0));
    /*
      WriteValue() expects a byte array but it must be packed into a
      tuple for the actual call through the proxy.
    */
    GVariant * const value_parameter = g_variant_new("(@aya{sv})", value, builder);
    g_variant_builder_unref(builder);
    GError * error = NULL;

    GVariant * const ret =
        g_dbus_proxy_call_sync(characteristic,
                               "WriteValue",
                               value_parameter,  // parameters
                               G_DBUS_CALL_FLAGS_NONE,
                               -1,    // timeout (default == -1),
                               NULL,  // cancellable
                               &error);

    if (ret == NULL)
    {
        OIC_LOG_V(ERROR,
                  TAG,
                  "[%p] WriteValue() call failed: %s",
                  characteristic,
                  error->message);

        g_error_free(error);

        ca_mutex_lock(context->lock);

        if (context->on_client_error != NULL)
        {
            /*
              At this point endpoint and send data information is
              available.
            */
            context->on_client_error(NULL,   // endpoint
                                     data,
                                     length,
                                     CA_STATUS_FAILED);
        }

        ca_mutex_unlock(context->lock);

        return CA_STATUS_FAILED;
    }

    g_variant_unref(ret);

    return CA_STATUS_OK;
}

CAResult_t CAGattClientSendData(char const * address,
                                uint8_t const * data,
                                size_t length,
                                CALEContext * context)
{
    assert(context != NULL);

    CAResult_t result = CA_STATUS_FAILED;

    ca_mutex_lock(g_context.lock);

    GDBusProxy * const characteristic =
        G_DBUS_PROXY(
            g_hash_table_lookup(g_context.characteristic_map,
                                address));

    if (characteristic == NULL)
    {
        /*
          GATT request characteristic corresponding to given address
          was not found.
        */

        ca_mutex_unlock(g_context.lock);

        return result;
    }

    result = CAGattClientSendDataImpl(characteristic,
                                      data,
                                      length,
                                      context);

    ca_mutex_unlock(g_context.lock);

    return result;
}

CAResult_t CAGattClientSendDataToAll(uint8_t const * data,
                                     size_t length,
                                     CALEContext * context)
{
    assert(context != NULL);

    CAResult_t result = CA_STATUS_FAILED;

    ca_mutex_lock(g_context.lock);

    if (g_hash_table_size (g_context.characteristic_map) == 0)
    {
        // Remote OIC GATT service was not found prior to getting here.
        ca_mutex_unlock(g_context.lock);
        return result;
    }

    GHashTableIter iter;
    g_hash_table_iter_init(&iter, g_context.characteristic_map);

    gpointer characteristic;  // Value

    /**
     * @todo Will content of this hash table be potentially changed by
     *       another thread during iteration?
     */
    while(g_hash_table_iter_next(&iter,
                                 NULL,  // Key - unused
                                 &characteristic))
    {
        result = CAGattClientSendDataImpl(G_DBUS_PROXY(characteristic),
                                          data,
                                          length,
                                          context);

        if (result != CA_STATUS_OK)
        {
            break;
        }
    }

    ca_mutex_unlock(g_context.lock);

    return result;
}

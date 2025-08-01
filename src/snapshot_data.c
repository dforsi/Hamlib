#define _XOPEN_SOURCE 700
#include <unistd.h>
#include "hamlib/config.h"
#include "hamlib/rig.h"
#include "hamlib/port.h"
#include "hamlib/rig_state.h"
#include "misc.h"
#include "cache.h"
#include "snapshot_data.h"
#include "hamlibdatetime.h"
#include "sprintflst.h"

#include "cJSON.h"

#define SPECTRUM_MODE_FIXED "FIXED"
#define SPECTRUM_MODE_CENTER "CENTER"

char snapshot_data_pid[20];

static int snapshot_serialize_rig(cJSON *rig_node, RIG *rig)
{
    cJSON *node;
    char buf[1024];
    struct rig_cache *cachep = CACHE(rig);
    struct rig_state *rs = STATE(rig);

    cJSON *id_node = cJSON_CreateObject();
    cJSON_AddStringToObject(id_node, "model", rig->caps->model_name);
    cJSON_AddStringToObject(id_node, "endpoint", RIGPORT(rig)->pathname);
    cJSON_AddStringToObject(id_node, "process", snapshot_data_pid);
    cJSON_AddStringToObject(id_node, "deviceId", rs->device_id);
    cJSON_AddItemToObject(rig_node, "id", id_node);

    node = cJSON_AddStringToObject(rig_node, "status",
                                   rig_strcommstatus(rs->comm_status));

    if (node == NULL)
    {
        goto error;
    }

    // TODO: need to store last error code
    node = cJSON_AddStringToObject(rig_node, "errorMsg", "");

    if (node == NULL)
    {
        goto error;
    }

    node = cJSON_AddStringToObject(rig_node, "name", rig->caps->model_name);

    if (node == NULL)
    {
        goto error;
    }

    node = cJSON_AddBoolToObject(rig_node, "split",
                                 cachep->split == RIG_SPLIT_ON ? 1 : 0);

    if (node == NULL)
    {
        goto error;
    }

    node = cJSON_AddStringToObject(rig_node, "splitVfo",
                                   rig_strvfo(cachep->split_vfo));

    if (node == NULL)
    {
        goto error;
    }

    node = cJSON_AddBoolToObject(rig_node, "satMode",
                                 cachep->satmode ? 1 : 0);

    if (node == NULL)
    {
        goto error;
    }

    rig_sprintf_mode(buf, sizeof(buf), rs->mode_list);
    char *p;
    cJSON *modes_array = cJSON_CreateArray();

    for (p = strtok(buf, " "); p; p = strtok(NULL, " "))
    {
        if (strlen(buf) > 0)
        {
            cJSON *tmp = cJSON_CreateString(p);
            cJSON_AddItemToArray(modes_array, tmp);
        }
    }

    cJSON_AddItemToObject(rig_node, "modes", modes_array);

    return RIG_OK;

error:
    RETURNFUNC2(-RIG_EINTERNAL);
}

// 128 max modes should last a while
#define MAX_MODES 128

static int snapshot_serialize_vfo(cJSON *vfo_node, RIG *rig, vfo_t vfo)
{
    freq_t freq;
    int freq_ms, mode_ms, width_ms;
    rmode_t mode;
    //rmode_t modes[MAX_MODES];
    pbwidth_t width;
    ptt_t ptt;
    split_t split;
    vfo_t split_vfo;
    int result;
    int is_rx, is_tx;
    cJSON *node;
    struct rig_cache *cachep = CACHE(rig);
    struct rig_state *rs = STATE(rig);

    // TODO: This data should match rig_get_info command response

    node = cJSON_AddStringToObject(vfo_node, "name", rig_strvfo(vfo));

    if (node == NULL)
    {
        goto error;
    }

    result = rig_get_cache(rig, vfo, &freq, &freq_ms, &mode, &mode_ms, &width,
                           &width_ms);

    if (result == RIG_OK)
    {
        node = cJSON_AddNumberToObject(vfo_node, "freq", freq);

        if (node == NULL)
        {
            goto error;
        }

        node = cJSON_AddStringToObject(vfo_node, "mode", rig_strrmode(mode));

        if (node == NULL)
        {
            goto error;
        }

        node = cJSON_AddNumberToObject(vfo_node, "width", (double) width);

        if (node == NULL)
        {
            goto error;
        }
    }

    split = cachep->split;
    split_vfo = cachep->split_vfo;

    is_rx = (split == RIG_SPLIT_OFF && vfo == rs->current_vfo)
            || (split == RIG_SPLIT_ON && vfo != split_vfo);
    is_tx = (split == RIG_SPLIT_OFF && vfo == rs->current_vfo)
            || (split == RIG_SPLIT_ON && vfo == split_vfo);
    ptt = cachep->ptt && is_tx;

    if (is_tx)
    {
        node = cJSON_AddBoolToObject(vfo_node, "ptt", ptt == RIG_PTT_OFF ? 0 : 1);
    }
    else
    {
        node = cJSON_AddBoolToObject(vfo_node, "ptt", 0);
    }

    if (node == NULL)
    {
        goto error;
    }


    node = cJSON_AddBoolToObject(vfo_node, "rx", is_rx);

    if (node == NULL)
    {
        goto error;
    }

    node = cJSON_AddBoolToObject(vfo_node, "tx", is_tx);

    if (node == NULL)
    {
        goto error;
    }

    return RIG_OK;

error:
    RETURNFUNC2(-RIG_EINTERNAL);
}

static int snapshot_serialize_spectrum(cJSON *spectrum_node, RIG *rig,
                                       struct rig_spectrum_line *spectrum_line)
{
    // Spectrum data is represented as a hexadecimal ASCII string where each data byte is represented as 2 ASCII letters
    char spectrum_data_string[HAMLIB_MAX_SPECTRUM_DATA * 2];
    cJSON *node;
    int i;
    struct rig_spectrum_scope *scopes = rig->caps->spectrum_scopes;
    char *name = "?";

    for (i = 0; scopes[i].name != NULL; i++)
    {
        if (scopes[i].id == spectrum_line->id)
        {
            name = scopes[i].name;
        }
    }

    node = cJSON_AddNumberToObject(spectrum_node, "id", spectrum_line->id);

    if (node == NULL)
    {
        goto error;
    }

    node = cJSON_AddStringToObject(spectrum_node, "name", name);

    if (node == NULL)
    {
        goto error;
    }

    node = cJSON_AddStringToObject(spectrum_node, "type",
                                   spectrum_line->spectrum_mode == RIG_SPECTRUM_MODE_CENTER ?
                                   SPECTRUM_MODE_CENTER : SPECTRUM_MODE_FIXED);

    if (node == NULL)
    {
        goto error;
    }

    node = cJSON_AddNumberToObject(spectrum_node, "minLevel",
                                   spectrum_line->data_level_min);

    if (node == NULL)
    {
        goto error;
    }

    node = cJSON_AddNumberToObject(spectrum_node, "maxLevel",
                                   spectrum_line->data_level_max);

    if (node == NULL)
    {
        goto error;
    }

    node = cJSON_AddNumberToObject(spectrum_node, "minStrength",
                                   spectrum_line->signal_strength_min);

    if (node == NULL)
    {
        goto error;
    }

    node = cJSON_AddNumberToObject(spectrum_node, "maxStrength",
                                   spectrum_line->signal_strength_max);

    if (node == NULL)
    {
        goto error;
    }

    node = cJSON_AddNumberToObject(spectrum_node, "centerFreq",
                                   spectrum_line->center_freq);

    if (node == NULL)
    {
        goto error;
    }

    node = cJSON_AddNumberToObject(spectrum_node, "span", spectrum_line->span_freq);

    if (node == NULL)
    {
        goto error;
    }

    node = cJSON_AddNumberToObject(spectrum_node, "lowFreq",
                                   spectrum_line->low_edge_freq);

    if (node == NULL)
    {
        goto error;
    }

    node = cJSON_AddNumberToObject(spectrum_node, "highFreq",
                                   spectrum_line->high_edge_freq);

    if (node == NULL)
    {
        goto error;
    }

    node = cJSON_AddNumberToObject(spectrum_node, "length",
                                   (double) spectrum_line->spectrum_data_length);

    if (node == NULL)
    {
        goto error;
    }

    to_hex(spectrum_line->spectrum_data_length, spectrum_line->spectrum_data,
           sizeof(spectrum_data_string), spectrum_data_string);
    node = cJSON_AddStringToObject(spectrum_node, "data", spectrum_data_string);

    if (node == NULL)
    {
        goto error;
    }

    return RIG_OK;

error:
    RETURNFUNC2(-RIG_EINTERNAL);
}

void snapshot_init()
{
    snprintf(snapshot_data_pid, sizeof(snapshot_data_pid), "%d", getpid());
}

int snapshot_serialize(size_t buffer_length, char *buffer, RIG *rig,
                       struct rig_spectrum_line *spectrum_line)
{
    cJSON *root_node;
    cJSON *rig_node, *vfos_array, *vfo_node, *spectra_array, *spectrum_node;
    cJSON *node;
    cJSON_bool bool_result;
    char buf[256];
    int result;
    int i;
    struct rig_state *rs = STATE(rig);

    root_node = cJSON_CreateObject();

    if (root_node == NULL)
    {
        RETURNFUNC2(-RIG_EINTERNAL);
    }

    node = cJSON_AddStringToObject(root_node, "app", PACKAGE_NAME);

    if (node == NULL)
    {
        goto error;
    }

    node = cJSON_AddStringToObject(root_node, "version",
                                   PACKAGE_VERSION " " HAMLIBDATETIME);

    if (node == NULL)
    {
        goto error;
    }

    node = cJSON_AddNumberToObject(root_node, "seq",
                                   rs->snapshot_packet_sequence_number);

    if (node == NULL)
    {
        goto error;
    }

    date_strget(buf, sizeof(buf), 0);
    node = cJSON_AddStringToObject(root_node, "time", buf);

    if (node == NULL)
    {
        goto error;
    }


    // TODO: Calculate 32-bit CRC of the entire JSON record replacing the CRC value with 0
    node = cJSON_AddNumberToObject(root_node, "crc", 0);

    if (node == NULL)
    {
        goto error;
    }

    rig_node = cJSON_CreateObject();

    if (rig_node == NULL)
    {
        goto error;
    }

    result = snapshot_serialize_rig(rig_node, rig);

    if (result != RIG_OK)
    {
        cJSON_Delete(rig_node);
        goto error;
    }

    cJSON_AddItemToObject(root_node, "rig", rig_node);

    vfos_array = cJSON_CreateArray();

    if (vfos_array == NULL)
    {
        goto error;
    }

    for (i = 0; i < HAMLIB_MAX_VFOS; i++)
    {
        vfo_t vfo = rs->vfo_list & RIG_VFO_N(i);

        if (!vfo)
        {
            continue;
        }

        vfo_node = cJSON_CreateObject();
        result = snapshot_serialize_vfo(vfo_node, rig, vfo);

        if (result != RIG_OK)
        {
            cJSON_Delete(vfo_node);
            goto error;
        }

        cJSON_AddItemToArray(vfos_array, vfo_node);
    }

    cJSON_AddItemToObject(root_node, "vfos", vfos_array);

    if (spectrum_line != NULL)
    {
        spectra_array = cJSON_CreateArray();

        if (spectra_array == NULL)
        {
            goto error;
        }

        spectrum_node = cJSON_CreateObject();
        result = snapshot_serialize_spectrum(spectrum_node, rig, spectrum_line);

        if (result != RIG_OK)
        {
            cJSON_Delete(spectrum_node);
            goto error;
        }

        cJSON_AddItemToArray(spectra_array, spectrum_node);

        cJSON_AddItemToObject(root_node, "spectra", spectra_array);
    }

    bool_result = cJSON_PrintPreallocated(root_node, buffer, (int) buffer_length,
                                          0);

    cJSON_Delete(root_node);

    if (!bool_result)
    {
        RETURNFUNC2(-RIG_EINVAL);
    }

    rs->snapshot_packet_sequence_number++;

    return RIG_OK;

error:
    cJSON_Delete(root_node);
    RETURNFUNC2(-RIG_EINTERNAL);
}

#include "meta.h"
#include "../coding/coding.h"

static int16_t SsPitchFromNote(int16_t note, int16_t fine, uint8_t center, uint8_t shift);

/* .VAB - standard PS1 bank format */
VGMSTREAM* init_vgmstream_vab(STREAMFILE* sf) {
    uint16_t programs, tones, wave_num;
    int16_t pitch;
    uint8_t center, shift;
    off_t programs_off, tones_off, waves_off, entry_off, data_offset;
    size_t data_size;
    int target_subsong = sf->stream_index, is_vh = 0, tone_num, tones_parsed;
    int i, j;
    STREAMFILE* sf_data = NULL;
    VGMSTREAM* vgmstream = NULL;

    /* this format is intended for storing samples for sequenced music but
     * some games use it for storing SFX as a hack */

    /* checks */
    if (!is_id32le(0x00, sf, "VABp"))
        goto fail;

    if (check_extensions(sf, "vh")) {
        is_vh = 1;
        sf_data = open_streamfile_by_ext(sf, "vb");
        if (!sf_data) goto fail;
    } else if (check_extensions(sf, "vab")) {
        is_vh = 0;
        sf_data = sf;
    } else {
        goto fail;
    }

    programs = read_u16le(0x12, sf);
    tones = read_u16le(0x14, sf);
    //waves = read_u16le(0x16, sf);

    programs_off = 0x20;
    tones_off = programs_off + 128 * 0x10;
    waves_off = tones_off + programs * 16 * 0x20;

    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > tones || tones < 1)
        goto fail;

    tones_parsed = 0;
    tone_num = target_subsong - 1;
    for (i = 0; i < programs; i++) {
        uint8_t program_tones;

        entry_off = programs_off + i * 0x10;
        program_tones = read_u8(entry_off + 0x00, sf);
        tones_parsed += program_tones;
        if (target_subsong - 1 < tones_parsed)
            break;

        tone_num -= program_tones;
    }

    entry_off = tones_off + i * 16 * 0x20 + tone_num * 0x20;
    center = read_u8(entry_off + 0x04, sf);
    shift = read_u8(entry_off + 0x05, sf);
    wave_num = read_u16le(entry_off + 0x16, sf);

    /* play default note */
    pitch = SsPitchFromNote(60, 0, center, shift);

    data_offset = is_vh ? 0x00 : (waves_off + 256 * 0x02);
    for (j = 0; j < wave_num; j++) {
        data_offset += read_u16le(waves_off + j * 0x02, sf) << 3;
    }

    data_size = read_u16le(waves_off + j * 0x02, sf) << 3;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(1, 0);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_VAB;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_none;
    vgmstream->sample_rate = (pitch * 44100) / 4096; // FIXME: Maybe use actual pitching if implemented.
    vgmstream->num_samples = ps_bytes_to_samples(data_size, 1);
    vgmstream->stream_size = data_size;
    vgmstream->num_streams = tones;
    snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%02d/%02d", i, tone_num);

    if (!vgmstream_open_stream(vgmstream, sf_data, data_offset))
        goto fail;

    if (is_vh) close_streamfile(sf_data);
    return vgmstream;

fail:
    if (is_vh) close_streamfile(sf_data);
    close_vgmstream(vgmstream);
    return NULL;
}

/* Converts VAB note to PS1 pitch value (0-4096 where 4096 is 44100 Hz).
 * Function reversed from PS1 SDK. */
static uint16_t _svm_ptable[] =
{
    4096, 4110, 4125, 4140, 4155, 4170, 4185, 4200,
    4216, 4231, 4246, 4261, 4277, 4292, 4308, 4323,
    4339, 4355, 4371, 4386, 4402, 4418, 4434, 4450,
    4466, 4482, 4499, 4515, 4531, 4548, 4564, 4581,
    4597, 4614, 4630, 4647, 4664, 4681, 4698, 4715,
    4732, 4749, 4766, 4783, 4801, 4818, 4835, 4853,
    4870, 4888, 4906, 4924, 4941, 4959, 4977, 4995,
    5013, 5031, 5050, 5068, 5086, 5105, 5123, 5142,
    5160, 5179, 5198, 5216, 5235, 5254, 5273, 5292,
    5311, 5331, 5350, 5369, 5389, 5408, 5428, 5447,
    5467, 5487, 5507, 5527, 5547, 5567, 5587, 5607,
    5627, 5648, 5668, 5688, 5709, 5730, 5750, 5771,
    5792, 5813, 5834, 5855, 5876, 5898, 5919, 5940,
    5962, 5983, 6005, 6027, 6049, 6070, 6092, 6114,
    6137, 6159, 6181, 6203, 6226, 6248, 6271, 6294,
    6316, 6339, 6362, 6385, 6408, 6431, 6455, 6478,
    6501, 6525, 6549, 6572, 6596, 6620, 6644, 6668,
    6692, 6716, 6741, 6765, 6789, 6814, 6839, 6863,
    6888, 6913, 6938, 6963, 6988, 7014, 7039, 7064,
    7090, 7116, 7141, 7167, 7193, 7219, 7245, 7271,
    7298, 7324, 7351, 7377, 7404, 7431, 7458, 7485,
    7512, 7539, 7566, 7593, 7621, 7648, 7676, 7704,
    7732, 7760, 7788, 7816, 7844, 7873, 7901, 7930,
    7958, 7987, 8016, 8045, 8074, 8103, 8133, 8162,
    8192
};

static int16_t SsPitchFromNote(int16_t note, int16_t fine, uint8_t center, uint8_t shift) {

    uint32_t pitch;
    int16_t calc, type;
    int32_t add, sfine;//, ret;

    sfine = fine + shift;
    if (sfine < 0) sfine += 7;
    sfine >>= 3;

    add = 0;
    if (sfine > 15) {
        add = 1;
        sfine -= 16;
    }

    calc = add + (note - (center - 60));//((center + 60) - note) + add;
    pitch = _svm_ptable[16 * (calc % 12) + (int16_t)sfine];
    type = calc / 12 - 5;

    // regular shift
    if (type > 0) return pitch << type;
    // negative shift
    if (type < 0) return pitch >> -type;

    return pitch;
}

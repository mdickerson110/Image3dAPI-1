#include "Image3dSource.hpp"


Image3dSource::Image3dSource() {
    m_probe.type = PROBE_THORAX;
    m_probe.name = L"4V";

    // One second loop starting at t = 10
    const size_t numFrames = 60;
    const double duration = 1.0; // Seconds
    const double fps = duration / numFrames;
    const double startTime = 10.0;

    {
        // simulate sine-wave ECG
        const int N = 128;
        CComSafeArray<float> samples(N);
        for (int i = 0; i < N; ++i)
            samples[i] = static_cast<float>(sin(4 * i*M_PI / N));

        CComSafeArray<double> trig_times;
        trig_times.Add(startTime); // trig every 1/2 sec
        trig_times.Add(startTime + duration / 2);
        trig_times.Add(startTime + duration);

        EcgSeries ecg;
        ecg.start_time = startTime;
        ecg.delta_time = duration / N;
        ecg.samples = samples.Detach();
        ecg.trig_times = trig_times.Detach();
        m_ecg = EcgSeries(ecg);
    }
    {
        // flat gray scale
        for (size_t i = 0; i < m_color_map.size(); ++i)
            m_color_map[i] = R8G8B8A8(static_cast<unsigned char>(i), static_cast<unsigned char>(i), static_cast<unsigned char>(i), 0xFF);
    }
    {
        // geometry          X     Y    Z
        Cart3dGeom geom = { -0.1f,-0.075f,  0,     // origin
            0.2f, 0,       0,     // dir1
            0,    0.15f,   0,     // dir2
            0,    0,       0.1f };// dir2
        m_geom = geom;
    }
    {
        // checker board image data
        unsigned short dims[] = { 12, 14, 16 };
        std::vector<byte> img_buf(dims[0] * dims[1] * dims[2]);
        for (size_t f = 0; f < numFrames; ++f) {
            for (unsigned int z = 0; z < dims[2]; ++z) {
                for (unsigned int y = 0; y < dims[1]; ++y) {
                    for (unsigned int x = 0; x < dims[0]; ++x) {
                        bool even_f = (f / 2 % 2) == 0;
                        bool even_x = (x / 2 % 2) == 0;
                        bool even_y = (y / 2 % 2) == 0;
                        bool even_z = (z / 2 % 2) == 0;
                        byte & out_sample = img_buf[x + y*dims[0] + z*dims[0] * dims[1]];
                        if (even_f ^ even_x ^ even_y ^ even_z)
                            out_sample = 255;
                        else
                            out_sample = 0;
                    }
                }
            }

            for (unsigned int y = 0; y < dims[1]; ++y) {
                for (unsigned int x = 0; x < dims[0]; ++x) {
                    unsigned int z = 0;
                    byte & out_sample = img_buf[x + y*dims[0] + z*dims[0] * dims[1]];
                    out_sample = 255; // Plane closest to probe is gray.
                }
            }

            Image3d tmp;
            {
                tmp.time = f * fps + startTime;
                tmp.format = FORMAT_U8;
                for (size_t i = 0; i < 3; ++i)
                    tmp.dims[i] = dims[i];
                CComSafeArray<BYTE> data(static_cast<unsigned int>(img_buf.size()));
                memcpy(data.m_psa->pvData, img_buf.data(), img_buf.size());
                tmp.data = data.Detach();
                tmp.stride0 = dims[0] * sizeof(byte);
                tmp.stride1 = dims[1] * tmp.stride0;
            }

            m_frames.push_back(std::move(tmp));
        }
    }
}

Image3dSource::~Image3dSource() {
}


HRESULT Image3dSource::GetFrameCount(/*out*/unsigned int *size) {
    if (!size)
        return E_INVALIDARG;

    *size = static_cast<unsigned int>(m_frames.size());
    return S_OK;
}

HRESULT Image3dSource::GetFrameTimes(/*out*/SAFEARRAY * *frame_times) {
    if (!frame_times)
        return E_INVALIDARG;

    const unsigned int N = static_cast<unsigned int>(m_frames.size());
    CComSafeArray<double> result(N);
    if (N > 0) {
        double * time_arr = &result.GetAt(0);
        for (unsigned int i = 0; i < N; ++i)
            time_arr[i] = m_frames[i].time;
    }

    *frame_times = result.Detach();
    return S_OK;
}

HRESULT Image3dSource::GetFrame(unsigned int index, Cart3dGeom geom, unsigned short max_res[3], /*out*/Image3d *data) {
    if (!data)
        return E_INVALIDARG;
    if (index >= m_frames.size())
        return E_BOUNDS;

    // \todo: Implement cropping of output image, based on input geom

    // return image data
#ifdef _WINDLL
    bool deep_copy = false; // don't copy when running in-process
#else
    bool deep_copy = true; // copy when communicating out-of-process
#endif
    *data = Image3d(m_frames[index]);
    return S_OK;
}

HRESULT Image3dSource::GetBoundingBox(/*out*/Cart3dGeom *geom) {
    if (!geom)
        return E_INVALIDARG;

    *geom = m_geom;
    return S_OK;
}

HRESULT Image3dSource::GetColorMap(/*out*/SAFEARRAY ** map) {
    if (!map)
        return E_INVALIDARG;
    if (*map)
        return E_INVALIDARG;

    // copy to new buffer
    CComSafeArray<unsigned int> color_map(static_cast<unsigned int>(m_color_map.size()));
    memcpy(&color_map.GetAt(0), m_color_map.data(), sizeof(m_color_map));
    *map = color_map.Detach(); // transfer ownership
    return S_OK;
}

HRESULT Image3dSource::GetECG(/*out*/EcgSeries *ecg) {
    if (!ecg)
        return E_INVALIDARG;

    // return a copy
    *ecg = EcgSeries(m_ecg);
    return S_OK;
}

HRESULT Image3dSource::GetProbeInfo(/*out*/ProbeInfo *probe) {
    if (!probe)
        return E_INVALIDARG;

    // return a copy
    *probe = m_probe;
    return S_OK;
}

HRESULT Image3dSource::GetSopInstanceUID(/*out*/BSTR *uid_str) {
    if (!uid_str)
        return E_INVALIDARG;
    if (*uid_str)
        return E_INVALIDARG; // input must be pointer to nullptr

    *uid_str = CComBSTR("DUMMY_UID").Detach();
    return S_OK;
}
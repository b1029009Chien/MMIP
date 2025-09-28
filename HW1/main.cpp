// Minimal image toolkit (pure std::C++): RAW(512x512, 8-bit gray), PGM/PPM(P5/P6), BMP(8/24-bit BI_RGB)
// Ops: negative / log / gamma, resize (nearest / bilinear)
// All pixels are row-major, interleaved (c = 1 or 3).
// Pixel-centered resampling: fx = (x+0.5)*sx - 0.5 (prevents half-pixel bias).
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <cmath>
#include <iomanip>
#include <cstring>

using namespace std;

// Image memory layout (row-major, interleaved):
// offset(i,j,k) = ((i * w) + j) * c + k
struct Image {
    // width, height, channels (1=PGM/RAW, 3=PPM)
    int w = 0, h = 0, c = 0;
    // size = w*h*c
    vector<uint8_t> data;
    bool empty() const { return data.empty(); }
};

struct Image;
static Image load_raw_grayscale(const string& path, int w, int h);
static Image load_bmp(const string& path);
static Image load_by_extension(const string& path);

// --- extension helpers (deal with file) ---
static string to_lower(string s) {
    for (char &ch : s) ch = (char)tolower((unsigned char)ch);
    return s;
}
static string file_ext(const string& path) {
    auto pos = path.find_last_of('.');
    if (pos == string::npos) return "";
    return to_lower(path.substr(pos));
}

// --------------------- RAW (8-bit gray) ---------------------
// load_raw_grayscale(path, w, h):
// Reads w*h bytes as 8-bit grayscale, row-major (no header, no padding).
// For this assignment, .raw means 512x512 single-channel
static Image load_raw_grayscale(const string& path, int w, int h) {
    Image img;
    img.w = w; img.h = h; img.c = 1;
    img.data.resize(static_cast<size_t>(w) * h);
    ifstream in(path, ios::binary);
    //error check
    if (!in) { cerr << "Cannot open RAW " << path << "\n"; img.data.clear(); return img; }
    // set false if bottom-up rows in file
    //use row-major order to read
    const bool file_is_top_down = true;
    vector<unsigned char> row(w);

    for (int i = 0; i < h; ++i) {
        // choose which destination row this file row maps to
        int dest_i = file_is_top_down ? i : (h - 1 - i);
        unsigned char* dst = &img.data[static_cast<size_t>(dest_i) * w];

        in.read(reinterpret_cast<char*>(row.data()), row.size());
        if (!in) { std::cerr << "RAW size mismatch\n"; img.data.clear(); return img; }

        // Copy row j=0..w-1 → row-major: offset = dest_i*w + j
        memcpy(dst, row.data(), row.size());
    }
    return img;
}

// --------------------- Utilities ---------------------
static void dump_center_10x10(const Image& img, const string& tag) {
    if (img.empty()) return;
    cout << "---- Center 10x10: " << tag << " (" << img.w << "x" << img.h << ", c=" << img.c << ") ----\n";
    const int cx = img.w / 2, cy = img.h / 2;
    const int x0 = max(0, cx - 5), y0 = max(0, cy - 5);
    const int x1 = min(img.w, x0 + 10), y1 = min(img.h, y0 + 10);

    auto get_gray = [&](int x, int y)->int {
        const uint8_t* p = &img.data[(static_cast<size_t>(y)*img.w + x)*img.c];
        if (img.c == 1) return p[0];
        // luminance for display
        return static_cast<int>(lround(0.299*p[0] + 0.587*p[1] + 0.114*p[2]));
    };

    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            cout << setw(4) << get_gray(x, y);
        }
        cout << "\n";
    }
    cout << "---------------------------------------------\n";
}

// --- Little-endian readers ---
static uint16_t rd_u16(istream& in) {
    unsigned char b[2]; in.read((char*)b, 2);
    return (uint16_t)(b[0] | (b[1] << 8));
}
static uint32_t rd_u32(istream& in) {
    unsigned char b[4]; in.read((char*)b, 4);
    return (uint32_t)(b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24));
}
static int32_t  rd_s32(istream& in) { return (int32_t)rd_u32(in); }

// bmp_row_size_bytes(bpp, w):
// rawBytes = ceil(bpp*w/8); pad = (4 - rawBytes%4)%4; return rawBytes + pad;
// Used by both reader and writer to honor 4-byte scanline alignment.
static inline int bmp_row_size_bytes(int bitsPerPixel, int w) {
    // 先算未含 padding 的位元組數（需對低於 8 bpp 進位到整數 byte）
    int rawBytes = (bitsPerPixel * w + 7) / 8;
    // 再補到 4 的倍數
    int pad      = (4 - (rawBytes % 4)) % 4;
    //cout << "Row size (bpp=" << bitsPerPixel << ", w=" << w << "): " << rawBytes << " + " << pad << " = " << (rawBytes + pad) << "\n";
    return rawBytes + pad;
}

// load_bmp():
// Supports BI_RGB only: 8-bit indexed (palette) and 24-bit BGR.
// Row stride is padded to 4 bytes; height < 0 => top-down.
// Converts to internal RGB (c=3). Palette entries are BGRA.
static Image load_bmp(const string& path) {
    Image img;
    ifstream in(path, ios::binary);
    if (!in) { cerr << "Cannot open BMP " << path << "\n"; return img; }

    // BITMAPFILEHEADER (14 bytes)
    char sigB = 0, sigM = 0;
    in.read(&sigB, 1); in.read(&sigM, 1);            // 'B','M' => 識別碼
    if (sigB != 'B' || sigM != 'M') {
        cerr << "Not a BMP: " << path << "\n"; return img;
    }
    (void) rd_u32(in);                                // file size (unused)
    (void) rd_u16(in); (void) rd_u16(in);            // reserved
    uint32_t offBits = rd_u32(in);                    // pixel data offset

    // DIB header (assume BITMAPINFOHEADER >= 40 bytes)
    uint32_t dibSize = rd_u32(in);
    if (dibSize < 40) { std::cerr << "Unsupported BMP DIB size\n"; return img; }

    int32_t  width  = rd_s32(in);
    int32_t  height = rd_s32(in);                     // <0 => top-down
    //cout << "BMP size: " << width << "x" << height << "\n";
    uint16_t planes = rd_u16(in);
    uint16_t bpp    = rd_u16(in);                     // 8 or 24
    uint32_t comp   = rd_u32(in);                     // 0=BI_RGB only
    (void) rd_u32(in);                                // image size (can be 0)
    (void) rd_s32(in); (void) rd_s32(in);             // xppm, yppm
    uint32_t clrUsed = rd_u32(in);                    // palette entries
    (void) rd_u32(in);                                // clrImportant

    if (planes != 1 || (bpp != 24 && bpp != 8) || comp != 0) {
        std::cerr << "BMP unsupported (bpp=" << bpp << ", comp=" << comp << ")\n";
        return img;
    }

    // We will output RGB (c=3) for both 24-bit and 8-bit indexed
    const int W = width;
    const int H = abs(height);
    const bool topDown = (height < 0);
    img.w = W; img.h = H; img.c = 3;
    img.data.assign((size_t)W * H * 3, 0);

    // Skip to palette or pixels
    // We have read 14 + 40 = 54 bytes so far; if dibSize > 40, skip the rest
    if (dibSize > 40) in.seekg((streamoff)(14 + dibSize), ios::beg);
    else in.seekg(54, ios::beg);

    // Palette for 8-bit
    vector<unsigned char> palette; // stored as BGRA quads
    if (bpp == 8) {
        uint32_t numColors = clrUsed ? clrUsed : 256;
        palette.resize((size_t)numColors * 4u);
        in.read((char*)palette.data(), (streamsize)palette.size());
    }

    // Ensure we're at pixel array start (offBits)
    in.seekg((streamoff)offBits, ios::beg);
    if (!in) { cerr << "BMP seek failed\n"; img.data.clear(); return img; }

    const int srcRow = bmp_row_size_bytes(bpp, W);

    vector<unsigned char> row(srcRow);

    for (int y = 0; y < H; ++y) {
        // Source row order: bottom-up if height>0, else top-down
        int srcY = topDown ? y : (H - 1 - y);
        streamoff pos = (streamoff)offBits + (streamoff)srcRow * srcY;
        //move to row start
        in.seekg(pos, ios::beg);
        in.read((char*)row.data(), srcRow);
        if (!in) { cerr << "BMP truncated row\n"; img.data.clear(); return img; }

        for (int x = 0; x < W; ++x) {
            unsigned char r=0,g=0,b=0;
            if (bpp == 24) {
                const unsigned char* p = &row[x * 3];
                b = p[0]; g = p[1]; r = p[2]; // BGR in file
            } else { // 8-bit indexed
                unsigned char idx = row[x];
                if (!palette.empty()) {
                    const unsigned char* q = &palette[(size_t)idx * 4u]; // BGRA
                    b = q[0]; g = q[1]; r = q[2];
                } else {
                    // No palette declared: treat index as gray
                    r = g = b = idx;
                }
            }
            size_t di = ((size_t)y * W + x) * 3;
            img.data[di+0] = r;
            img.data[di+1] = g;
            img.data[di+2] = b;
        }
    }
    return img;
}

static inline uint8_t clamp_u8f(float v) {
    if (v < 0.f) v = 0.f;
    if (v > 255.f) v = 255.f;
    return static_cast<uint8_t>(lround(v));
}

// --------------------- Point operations ---------------------
// negative: v -> 255 - v  (can use C-style pointer loop or 256-entry LUT)
// log:      s = (255/log(256))*log(1+v)      (use 256-entry LUT to avoid per-pixel log)
// gamma:    s = 255 * (v/255)^gamma          (use 256-entry LUT; apply per byte)
static Image op_negative(const Image& in) {
    Image out = in;
    uint8_t* p = out.data.data();
    uint8_t* e = p + out.data.size();
    while (p < e) {
        *p = static_cast<uint8_t>(255 - *p);
        ++p;
    }
    return out;
}

static Image op_log(const Image& in) {
    // s = c * log(1 + r), r in [0,255], c = 255 / log(256)
    Image out = in;
    // Precompute once
    uint8_t log_lut[256];
    {
        const float c = 255.0f / log(256.0f);
        for (int i = 0; i < 256; ++i) {
            float s = c * log(1.0f + float(i));
            log_lut[i] = clamp_u8f(s);
        }
    }

    for (size_t idx = 0; idx < out.data.size(); ++idx) {
        out.data[idx] = log_lut[out.data[idx]];
    }
    return out;
}

static Image op_gamma(const Image& in, float gamma) {
    uint8_t lut[256];
    for (int i = 0; i < 256; ++i) {
        float r = static_cast<float>(i) / 255.0f;
        float s = std::pow(r, gamma) * 255.0f;
        if (s < 0.0f) s = 0.0f;
        if (s > 255.0f) s = 255.0f;
        lut[i] = static_cast<uint8_t>(std::lround(s));
    }

    Image out = in;
    for (size_t idx = 0; idx < out.data.size(); ++idx) {
        out.data[idx] = lut[out.data[idx]];
    }
    return out;
}


// --------------------- Resizing ---------------------
// clamp_val(v, lo, hi): local clamp for pre-C++17 compilers.
// Also include <cstring> for std::memcpy; <cctype> for std::tolower.
template <typename T>
static inline T clamp_val(T v, T lo, T hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}
//--------------------- NN resize ---------------------
// resize_nearest(in, newW, newH):
// Pixel-centered mapping: fx=(x+0.5)*sx - 0.5, fy=(y+0.5)*sy - 0.5.
// Round to nearest source index; clamp at borders.
// Very fast; produces blockiness when upscaling.
static Image resize_nearest(const Image& in, int newW, int newH) {
    Image out; out.w = newW; out.h = newH; out.c = in.c;
    out.data.resize(static_cast<size_t>(newW) * newH * out.c);
    const double sx = static_cast<double>(in.w) / newW;
    const double sy = static_cast<double>(in.h) / newH;

    for (int y = 0; y < newH; ++y) {
        int syi = (int)floor((y + 0.5) * sy - 0.5);
        syi = clamp_val(syi, 0, in.h - 1);
        for (int x = 0; x < newW; ++x) {
            int sxi = (int)floor((x + 0.5) * sx - 0.5);
            sxi = clamp_val(sxi, 0, in.w - 1);
            const uint8_t* sp = &in.data[(static_cast<size_t>(syi)*in.w + sxi)*in.c];
            uint8_t* dp = &out.data[(static_cast<size_t>(y)*newW + x)*out.c];
            for (int ch = 0; ch < out.c; ++ch) dp[ch] = sp[ch];
        }
    }
    return out;
}

//--------------------- Bilinear resize ---------------------
// resize_bilinear(in, newW, newH):
// Let x0=floor(fx), x1=x0+1, wx=fx-x0 (same for y).
// v0=(1-wx)*F(x0,y0) + wx*F(x1,y0)
// v1=(1-wx)*F(x0,y1) + wx*F(x1,y1)
// v =(1-wy)*v0       + wy*v1
// Weights sum to 1; clamp indices; per-channel blend then clamp_u8f().
static Image resize_bilinear(const Image& in, int newW, int newH) {
    Image out; out.w = newW; out.h = newH; out.c = in.c;
    out.data.resize(static_cast<size_t>(newW) * newH * out.c);
    const double scaleX = static_cast<double>(in.w) / newW;
    const double scaleY = static_cast<double>(in.h) / newH;

    for (int y = 0; y < newH; ++y) {
        double fy = (y + 0.5) * scaleY - 0.5;
        int y0 = static_cast<int>(floor(fy));
        int y1 = y0 + 1;
        double wy = fy - y0;
        y0 = clamp_val(y0, 0, in.h - 1);
        y1 = clamp_val(y1, 0, in.h - 1);

        for (int x = 0; x < newW; ++x) {
            double fx = (x + 0.5) * scaleX - 0.5;
            int x0 = static_cast<int>(floor(fx));
            int x1 = x0 + 1;
            double wx = fx - x0;
            x0 = clamp_val(x0, 0, in.w - 1);
            x1 = clamp_val(x1, 0, in.w - 1);

            for (int ch = 0; ch < out.c; ++ch) {
                const int idx00 = ((y0*in.w + x0)*in.c + ch);
                const int idx10 = ((y0*in.w + x1)*in.c + ch);
                const int idx01 = ((y1*in.w + x0)*in.c + ch);
                const int idx11 = ((y1*in.w + x1)*in.c + ch);

                double v00 = in.data[idx00];
                double v10 = in.data[idx10];
                double v01 = in.data[idx01];
                double v11 = in.data[idx11];

                double v0 = v00 * (1.0 - wx) + v10 * wx;
                double v1 = v01 * (1.0 - wx) + v11 * wx;
                double v  = v0  * (1.0 - wy) + v1  * wy;

                out.data[(static_cast<size_t>(y)*newW + x)*out.c + ch] = clamp_u8f(static_cast<float>(v));
            }
        }
    }
    return out;
}
// --------------------- PNM (PGM/PPM) ---------------------
static bool write_pnm(const string& path, const Image& img) {
    if (img.empty()) return false;
    const bool isGray = (img.c == 1);
    ofstream out(path, ios::binary);
    if (!out) { cerr << "Cannot write " << path << "\n"; return false; }
    out << (isGray ? "P5\n" : "P6\n")
        << img.w << " " << img.h << "\n"
        << 255 << "\n";
    out.write(reinterpret_cast<const char*>(img.data.data()), img.data.size());
    return static_cast<bool>(out);
}
// ------- BMP writer (BI_RGB; 24-bit for RGB, 8-bit paletted for gray) -------
static bool write_bmp(const std::string& path, const Image& img) {
    if (img.empty()) return false;

    const int W = img.w, H = img.h;
    const bool isGray = (img.c == 1);

    const int bpp = isGray ? 8 : 24;
    const int rowSize = bmp_row_size_bytes(bpp, W);
    const int pixelArraySize = rowSize * H;

    // Headers
    const uint32_t bfOffBits = 14 + 40 + (isGray ? 256 * 4 : 0); // file + DIB + palette
    const uint32_t bfSize    = bfOffBits + pixelArraySize;

    std::ofstream out(path, std::ios::binary);
    if (!out) { std::cerr << "Cannot write " << path << "\n"; return false; }

    auto wr_u16 = [&](uint16_t v){ out.put((char)(v & 0xFF)); out.put((char)(v >> 8)); };
    auto wr_u32 = [&](uint32_t v){
        out.put((char)( v        & 0xFF));
        out.put((char)((v >> 8 ) & 0xFF));
        out.put((char)((v >> 16) & 0xFF));
        out.put((char)((v >> 24) & 0xFF));
    };
    auto wr_s32 = [&](int32_t v){ wr_u32((uint32_t)v); };

    // BITMAPFILEHEADER (14)
    out.put('B'); out.put('M');
    wr_u32(bfSize);
    wr_u16(0); wr_u16(0);     // reserved
    wr_u32(bfOffBits);

    // BITMAPINFOHEADER (40)
    wr_u32(40);               // biSize
    wr_s32(W);
    wr_s32(H);                // positive -> bottom-up
    wr_u16(1);                // planes
    wr_u16((uint16_t)bpp);
    wr_u32(0);                // BI_RGB
    wr_u32(pixelArraySize);
    wr_s32(2835);             // ~72 DPI (optional)
    wr_s32(2835);
    wr_u32(isGray ? 256u : 0u); // colors used
    wr_u32(0);

    // Palette for 8-bit (grayscale ramp, BGRA)
    if (isGray) {
        for (int i = 0; i < 256; ++i) {
            unsigned char b = (unsigned char)i;
            out.put((char)b); out.put((char)b); out.put((char)b); out.put((char)0);
        }
    }

    // Pixel data (bottom-up)
    std::vector<unsigned char> row(rowSize, 0);
    for (int y = H - 1; y >= 0; --y) {           // write bottom row first
        if (isGray) {
            // indices directly from grayscale
            const unsigned char* src = &img.data[(size_t)y * W];
            std::memcpy(row.data(), src, (size_t)W);
        } else {
            // convert RGB -> BGR in file
            const unsigned char* src = &img.data[(size_t)y * W * 3];
            for (int x = 0; x < W; ++x) {
                row[x*3 + 0] = src[x*3 + 2]; // B
                row[x*3 + 1] = src[x*3 + 1]; // G
                row[x*3 + 2] = src[x*3 + 0]; // R
            }
        }
        out.write((const char*)row.data(), rowSize);
        if (!out) { std::cerr << "BMP write row failed\n"; return false; }
    }
    return true;
}

// load_by_extension(path):
//   .bmp      -> load_bmp()
//   .raw      -> load_raw_grayscale(512,512)
//   .jpg/.png -> not supported in stdlib build (print conversion hint)
static Image load_by_extension(const string& path) {
    const string ext = file_ext(path);
    if (ext == ".bmp") {
        return load_bmp(path);
    } else if (ext == ".raw") {
        return load_raw_grayscale(path, 512, 512);
    } else if (ext == ".jpg" || ext == ".jpeg" || ext == ".png") {
        cerr << "Note: JPEG/PNG need a decoder. Convert to BMP/RAW first (e.g., `magick input.jpg -colorspace RGB output.bmp`).\n";
        return Image{};
    } else {
        cerr << "Unknown extension: " << ext << "\n";
        return Image{};
    }
}

static bool write_by_extension(const std::string& path, const Image& img) {
    const std::string ext = file_ext(path);
    if (ext == ".pgm" || ext == ".ppm") {
        return write_pnm(path, img);
    } else if (ext == ".bmp") {
        return write_bmp(path, img);
    } else {
        // default to PNM if no/unknown extension
        std::cerr << "Unknown output extension '" << ext
                  << "'. Writing PNM instead.\n";
        return write_pnm(path, img);
    }
}

// ---------------------- [CLI / USAGE] ----------------------
// Commands:
//   read    <in.(bmp|raw|jpg|jpeg|png)> <out.(pgm|ppm|bmp)>
//   enhance <neg|log|gamma> [gamma] <in.(bmp|raw)> <out.(pgm|ppm|bmp)>
//   resize  <nearest|bilinear> <in|W> <W|in> <H> <out>
// Notes:
//   - .raw is 512x512 8-bit gray by convention.
//   - JPEG/PNG: not decoded in stdlib build; convert externally.
//   - resize accepts both arg orders (in,W,H,out) or (W,H,in,out).
static void usage() {
    cerr <<
    "Usage:\n"
    "  Read:       main read <in.(bmp|raw|jpg|jpeg|png)> <out.(pgm|ppm|bmp)>\n"
    "  Enhance:    main enhance <neg|log|gamma> [gamma] <in.(bmp|raw)> <out.(pgm|ppm|bmp)>\n"
    "  Resize:     main resize <nearest|bilinear> <in.(bmp|raw)> <newW> <newH> <out.(pgm|ppm|bmp)>\n";
}

// parse_int_strict(s, out): returns true if s is a valid integer (no trailing junk), stores result in out
static bool parse_int_strict(const std::string& s, int& out) {
    char* end = nullptr;
    long val = std::strtol(s.c_str(), &end, 10);
    if (!s.empty() && end == s.c_str() + s.size()) {
        out = static_cast<int>(val);
        return true;
    }
    return false;
}

int main(int argc, char** argv) {
    if (argc < 2) { usage(); return 1; }
    const string cmd = argv[1];

    if (cmd == "read") {
        if (argc != 4) { usage(); return 1; }
        const string inpath = argv[2], outpath = argv[3];
        Image im = load_by_extension(inpath);
        if (im.empty()) return 1;
        dump_center_10x10(im, "original");
        if (!write_by_extension(outpath, im)) { cerr << "Write failed\n"; return 1; }
        cout << "Saved: " << outpath << "\n";
        return 0;
    }

    if (cmd == "enhance") {
        if (argc < 5) { usage(); return 1; }
        const string op = argv[2];

        string inpath, outpath;
        float gamma = 1.0f;

        if (op == "gamma") {
            if (argc != 6) { usage(); return 1; }
            gamma  = stof(argv[3]);
            inpath = argv[4];
            outpath= argv[5];
        } else {
            if (argc != 5) { usage(); return 1; }
            inpath = argv[3];
            outpath= argv[4];
        }

        Image im = load_by_extension(inpath);
        if (im.empty()) return 1;

        Image out;
        if      (op == "neg")   out = op_negative(im);
        else if (op == "log")   out = op_log(im);
        else if (op == "gamma") out = op_gamma(im, gamma);
        else { usage(); return 1; }

        dump_center_10x10(out, "enhanced");
        if (!write_by_extension(outpath, out)) { std::cerr << "Write failed\n"; return 1; }
        std::cout << "Saved: " << outpath << "\n";
        return 0;
    }

    if (cmd == "resize") {
        if (argc != 7) { usage(); return 1; }
        const std::string mode = argv[2];

        // Accept both:
        //  A) resize <mode> <in> <W> <H> <out>
        //  B) resize <mode> <W> <H> <in> <out>
        std::string inpath, outpath;
        int newW = 0, newH = 0;

        int tmpW = 0, tmpH = 0;
        bool aW = parse_int_strict(argv[3], tmpW);
        bool aH = parse_int_strict(argv[4], tmpH);

        if (aW && aH) {
            // Form B
            newW   = tmpW;
            newH   = tmpH;
            inpath = argv[5];
            outpath= argv[6];
        } else {
            // Form A
            inpath = argv[3];
            if (!parse_int_strict(argv[4], newW) || !parse_int_strict(argv[5], newH)) {
                std::cerr << "Width/Height must be integers.\n"; return 1;
            }
            outpath= argv[6];
        }

        if (newW <= 0 || newH <= 0) { std::cerr << "Width/Height must be > 0.\n"; return 1; }

        Image im = load_by_extension(inpath);
        if (im.empty()) return 1;

        Image out;
        if      (mode == "nearest")  out = resize_nearest(im, newW, newH);
        else if (mode == "bilinear") out = resize_bilinear(im, newW, newH);
        else { usage(); return 1; }

        dump_center_10x10(out, "resized");
        if (!write_by_extension(outpath, out)) { std::cerr << "Write failed\n"; return 1; }
        std::cout << "Saved: " << outpath << "\n";
        return 0;
    }


    usage();
    return 1;
}
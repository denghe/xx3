#include <xx.h>
#include <fstream>
#include <zstd.h>

//#define STBI_NO_JPEG
//#define STBI_NO_PNG
#define STBI_NO_GIF
#define STBI_NO_PSD
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STBI_NO_HDR
#define STBI_NO_TGA
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#define STB_RECT_PACK_IMPLEMENTATION
#include <stb_rect_pack.h>

namespace xx {
	
    /**************************************************************************************************/
    // xx_file.h
    /**************************************************************************************************/

    Data ReadAllBytes(std::filesystem::path const& path) {
		std::ifstream f(path, std::ifstream::binary);
		if (!f) return {};													// not found? no permission? locked?
        auto sg = MakeScopeGuard([&] { f.close(); });
		f.seekg(0, f.end);
		auto&& siz = f.tellg();
		if ((uint64_t)siz > std::numeric_limits<size_t>::max()) return {};	// too big
		f.seekg(0, f.beg);
		auto outBuf = std::make_unique_for_overwrite<uint8_t[]>(siz);		// maybe throw oom
        Data d;
        d.Resize(siz);
		f.read((char*)d.buf, siz);
		if (!f) return {};													// only f.gcount() could be read
        return d;
	}

    Data ReadAllBytes_sv(std::string_view path) {
        if (path.empty()) return {};
        return ReadAllBytes((std::u8string_view&)path);
    }

	int WriteAllBytes(std::filesystem::path const& path, char const* buf, size_t len) {
		std::ofstream f(path, std::ios::binary | std::ios::trunc);
		if (!f) return -1;						// no create permission? exists readonly?
        auto sg = MakeScopeGuard([&] { f.close(); });
		f.write(buf, len);
		if (!f) return -2;						// write error
		return 0;
	}
	

    // .exe + 50k
    void ZstdDecompress(std::string_view const& src, Data& dst) {
        auto&& siz = ZSTD_getFrameContentSize(src.data(), src.size());
        if (ZSTD_CONTENTSIZE_UNKNOWN == siz) throw std::logic_error("ZstdDecompress error: unknown content size.");
        if (ZSTD_CONTENTSIZE_ERROR == siz) throw std::logic_error("ZstdDecompress read content size error.");
        dst.Resize(siz);
        if (0 == siz) return;
        siz = ZSTD_decompress(dst.buf, siz, src.data(), src.size());
        if (ZSTD_isError(siz)) throw std::logic_error("ZstdDecompress decompress error.");
        dst.Resize(siz);
    }

    void TryZstdDecompress(Data& d) {
        if (d.len >= 4) {
            if (d[0] == 0x28 && d[1] == 0xB5 && d[2] == 0x2F && d[3] == 0xFD) {
                Data d2;
                ZstdDecompress(d, d2);
                std::swap(d, d2);
            }
        }
    }

    // .exe + 320k
    void ZstdCompress(std::string_view const& src, Data& dst, int level, bool doShrink) {
        auto req = ZSTD_compressBound(src.size());
        dst.Resize(req);
        dst.len = ZSTD_compress(dst.buf, dst.cap, src.data(), src.size(), level);
        if (doShrink) {
            dst.Shrink();
        }
    }



    /**************************************************************************************************/
    // xx_math.h
    /**************************************************************************************************/


	bool IsIntersect_BoxBoxF(XY b1minXY, XY b1maxXY, XY b2minXY, XY b2maxXY) {
		return !(b1maxXY.x < b2minXY.x || b2maxXY.x < b1minXY.x
			|| b1maxXY.y < b2minXY.y || b2maxXY.y < b1minXY.y);
	}

	bool IsIntersect_BoxPointF(XY b1minXY, XY b1maxXY, XY p) {
		return !(b1maxXY.x < p.x || p.x < b1minXY.x || b1maxXY.y < p.y || p.y < b1minXY.y);
	}

	float CalcBounce(float x) {
		return 1.f - std::expf(-5.f * x) * std::cosf(6.f * (float)M_PI * x);
	}

	XY GetRndPosDoughnut(Rnd& rnd_, float maxRadius_, float safeRadius_, float radiansFrom_, float radiansTo_) {
		auto len = maxRadius_ - safeRadius_;
		auto len_radius = len / maxRadius_;
		auto safeRadius_radius = safeRadius_ / maxRadius_;
		auto radius = std::sqrtf(rnd_.Next<float>(0, len_radius) + safeRadius_radius) * maxRadius_;
		auto radians = rnd_.Next<float>(radiansFrom_, radiansTo_);
		return { std::cosf(radians) * radius, std::sinf(radians) * radius };
	}


    /**************************************************************************************************/
    // xx_grid2daabb.h
    /**************************************************************************************************/

    void Grid2dAABB::Init(XYi gridSize_, XY cellSize_, int32_t cellCap_, int32_t capacity_) {
        assert(!nodes && capacity_ > 0);
        assert(gridSize_.x > 0 && gridSize_.y > 0);
        assert(cellSize_.x > 0 && cellSize_.y > 0);
        gridSize = gridSize_;
        cellSize = cellSize_;
        _1_cellSize = 1.f / cellSize_;
        worldSize = cellSize_ * gridSize_;
        freeHead = -1;
        freeCount = count = 0;
        capacity = capacity_;
        nodes = (Node*)new MyAlignedStorage<Node>[capacity_];
        cells.Resize(gridSize_.x * gridSize_.y);
        for (auto& c : cells) c.Reserve(cellCap_);
    }

    Grid2dAABB::~Grid2dAABB() {
        if (!nodes) return;
        delete[](MyAlignedStorage<Node>*)nodes;
        nodes = {};
    }

	void Grid2dAABB::Reserve(int32_t capacity_) {
		assert(capacity_ > 0);
		if (capacity_ <= capacity) return;
		auto newNodes = (Node*)new MyAlignedStorage<Node>[capacity_];
		::memcpy((void*)newNodes, (void*)nodes, count * sizeof(Node));
		delete[](MyAlignedStorage<Node>*)nodes;
		nodes = newNodes;
		capacity = capacity_;
	}

    int32_t Grid2dAABB::Add(FromTo<XY> const& aabb_, void* ud_) {
        assert(aabb_.from.x < aabb_.to.x && aabb_.from.y < aabb_.to.y);
        assert(aabb_.from.x >= 0 && aabb_.from.x < worldSize.x);
        assert(aabb_.from.y >= 0 && aabb_.from.y < worldSize.y);
        assert(aabb_.to.x >= 0 && aabb_.to.x < worldSize.x);
        assert(aabb_.to.y >= 0 && aabb_.to.y < worldSize.y);

        // alloc
        int32_t nodeIndex;
        if (freeCount > 0) {
            nodeIndex = freeHead;
            freeHead = nodes[nodeIndex].next;
            freeCount--;
        }
        else {
            if (count == capacity) {
                Reserve(count ? count * 2 : 16);
            }
            nodeIndex = count;
            count++;
        }
        auto& o = nodes[nodeIndex];

        // calc covered cells
        FromTo<XYi> ccrr{ (aabb_.from * _1_cellSize).template As<int32_t>()
                        , (aabb_.to * _1_cellSize).template As<int32_t>() };

        // link
        for (auto y = ccrr.from.y; y <= ccrr.to.y; y++) {
            for (auto x = ccrr.from.x; x <= ccrr.to.x; x++) {
                auto& c = cells[y * gridSize.x + x];
                assert(c.Find(nodeIndex) == -1);
                c.Emplace(nodeIndex);
            }
        }

        // init
        o.next = -1;
        o.inResults = 0;
        o.aabb = aabb_;
        o.ccrr = ccrr;
		o.ud = ud_;
        return nodeIndex;
    }

    void Grid2dAABB::Remove(int32_t nodeIndex_) {
        assert(nodeIndex_ >= 0 && nodeIndex_ < count && nodes[nodeIndex_].next == -1);
        auto& o = nodes[nodeIndex_];

        // unlink
        for (auto y = o.ccrr.from.y; y <= o.ccrr.to.y; y++) {
            for (auto x = o.ccrr.from.x; x <= o.ccrr.to.x; x++) {
                auto& c = cells[y * gridSize.x + x];
                for (int32_t i = c.len - 1; i >= 0; --i) {
                    if (c[i] == nodeIndex_) {
                        c.SwapRemoveAt(i);
                        goto LabEnd;
                    }
                }
                assert(false);
            LabEnd:;
            }
        }

        // free
        o.next = freeHead;
        freeHead = nodeIndex_;
        freeCount++;
		o.ud = {};
    }

    void Grid2dAABB::Update(int32_t nodeIndex_, FromTo<XY> const& aabb_) {
        assert(nodes);
        assert(nodeIndex_ >= 0 && nodeIndex_ < count && nodes[nodeIndex_].next == -1);
        assert(aabb_.from.x < aabb_.to.x && aabb_.from.y < aabb_.to.y);
        assert(aabb_.from.x >= 0 && aabb_.from.x < worldSize.x);
        assert(aabb_.from.y >= 0 && aabb_.from.y < worldSize.y);
        assert(aabb_.to.x >= 0 && aabb_.to.x < worldSize.x);
        assert(aabb_.to.y >= 0 && aabb_.to.y < worldSize.y);

        // locate
        auto& o = nodes[nodeIndex_];

        // update1
        o.aabb = aabb_;

        FromTo<XYi> ccrr{ (aabb_.from * _1_cellSize).template As<int32_t>()
                        , (aabb_.to * _1_cellSize).template As<int32_t>() };

        // no change check
        if (o.ccrr == ccrr) return;

        // unlink
        for (auto y = o.ccrr.from.y; y <= o.ccrr.to.y; y++) {
            for (auto x = o.ccrr.from.x; x <= o.ccrr.to.x; x++) {
                auto& c = cells[y * gridSize.x + x];
                for (int32_t i = c.len - 1; i >= 0; --i) {
                    if (c[i] == nodeIndex_) {
                        c.SwapRemoveAt(i);
                        goto LabEnd;
                    }
                }
                assert(false);
            LabEnd:;
            }
        }

        // link
        for (auto y = ccrr.from.y; y <= ccrr.to.y; y++) {
            for (auto x = ccrr.from.x; x <= ccrr.to.x; x++) {
                auto& c = cells[y * gridSize.x + x];
                assert(c.Find(nodeIndex_) == -1);
                c.Emplace(nodeIndex_);
            }
        }

        // update2
        o.ccrr = ccrr;
    }

	Grid2dAABB::Node& Grid2dAABB::NodeAt(int32_t nodeIndex_) const {
        assert(nodeIndex_ >= 0 && nodeIndex_ < count && nodes[nodeIndex_].next == -1);
        return (Grid2dAABB::Node&)nodes[nodeIndex_];
    }

    int32_t Grid2dAABB::Count() const {
        return count - freeCount;
    }

    bool Grid2dAABB::Empty() const {
        return Count() == 0;
    }


    bool Grid2dAABB::TryLimitAABB(FromTo<XY>& aabb_, float edge_) {
        assert(edge_ > 0 && edge_ < cellSize.x * 0.5f && edge_ < cellSize.y * 0.5f);
        auto ws = worldSize - edge_;
        if (!IsIntersect_BoxBoxF(XY{ edge_ }, ws, aabb_.from, aabb_.to)) return false;
        if (aabb_.from.x < edge_) aabb_.from.x = edge_;
        if (aabb_.from.y < edge_) aabb_.from.y = edge_;
        if (aabb_.to.x > ws.x) aabb_.to.x = ws.x;
        if (aabb_.to.y > ws.y) aabb_.to.y = ws.y;
        return true;
    }

    void Grid2dAABB::ForeachPoint(XY const& p_) {
        assert(p_.x >= 0 && p_.x < worldSize.x);
        assert(p_.y >= 0 && p_.y < worldSize.y);
        results.Clear();

        auto cr = (p_ * _1_cellSize).template As<int32_t>();
        auto& c = cells[cr.y * gridSize.x + cr.x];
        for (int32_t i = 0; i < c.len; ++i) {
            auto& n = nodes[c[i]];
            if (IsIntersect_BoxPointF(n.aabb.from, n.aabb.to, p_)) {
                results.Add(c[i]);
            }
        }
    }

    void Grid2dAABB::ForeachAABB(FromTo<XY> const& aabb_, int32_t except_) {
        assert(aabb_.from.x < aabb_.to.x && aabb_.from.y < aabb_.to.y);
        assert(aabb_.from.x >= 0 && aabb_.from.x < worldSize.x);
        assert(aabb_.from.y >= 0 && aabb_.from.y < worldSize.y);
        assert(aabb_.to.x >= 0 && aabb_.to.x < worldSize.x);
        assert(aabb_.to.y >= 0 && aabb_.to.y < worldSize.y);
        results.Clear();

        // calc covered cells
        FromTo<XYi> ccrr{ (aabb_.from * _1_cellSize).template As<int32_t>()
            , (aabb_.to * _1_cellSize).template As<int32_t>() };

        if (ccrr.from.x == ccrr.to.x || ccrr.from.y == ccrr.to.y) {
            // 1-2 row, 1-2 col: do full rect check
            for (auto y = ccrr.from.y; y <= ccrr.to.y; y++) {
                for (auto x = ccrr.from.x; x <= ccrr.to.x; x++) {
                    auto& c = cells[y * gridSize.x + x];
                    for (int32_t i = 0; i < c.len; ++i) {
                        if (c[i] == except_) continue;
                        auto& n = nodes[c[i]];
                        if (!(n.aabb.to.x < aabb_.from.x || aabb_.to.x < n.aabb.from.x
                            || n.aabb.to.y < aabb_.from.y || aabb_.to.y < n.aabb.from.y)) {
                            results.Emplace(c[i]);
                        }
                    }
                }
            }
            return;
        }

        // except set flag
        if (except_ != -1) {
            assert(except_ >= 0 && except_ < count && nodes[except_].next == -1);
            nodes[except_].inResults = 1;
        }

        // first row: check AB
        auto y = ccrr.from.y;

        // first cell: check top left AB
        auto x = ccrr.from.x;
        {
            auto& c = cells[y * gridSize.x + x];
            for (int32_t i = 0; i < c.len; ++i) {
                auto& n = nodes[c[i]];
                if (n.inResults) continue;
                if (n.aabb.to.x > aabb_.from.x && n.aabb.to.y > aabb_.from.y) {
                    n.inResults = 1;
                    results.Emplace(c[i]);
                }
            }
        }

        // middle cells: check top AB
        for (++x; x < ccrr.to.x; x++) {
            auto& c = cells[y * gridSize.x + x];
            for (int32_t i = 0; i < c.len; ++i) {
                auto& n = nodes[c[i]];
                if (n.inResults) continue;
                if (n.aabb.to.y > aabb_.from.y) {
                    n.inResults = 1;
                    results.Emplace(c[i]);
                }
            }
        }

        // last cell: check top right AB
        if (x == ccrr.to.x) {
            auto& c = cells[y * gridSize.x + x];
            for (int32_t i = 0; i < c.len; ++i) {
                auto& n = nodes[c[i]];
                if (n.inResults) continue;
                if (n.aabb.from.x < aabb_.to.x && n.aabb.to.y > aabb_.from.y) {
                    n.inResults = 1;
                    results.Emplace(c[i]);
                }
            }
        }

        // middle rows: first & last col check AB
        for (++y; y < ccrr.to.y; y++) {

            // first cell: check left AB
            x = ccrr.from.x;
            auto& c = cells[y * gridSize.x + x];
            for (int32_t i = 0; i < c.len; ++i) {
                auto& n = nodes[c[i]];
                if (n.inResults) continue;
                if (n.aabb.to.x > aabb_.from.x) {
                    n.inResults = 1;
                    results.Emplace(c[i]);
                }
            }

            // middle cols: no check
            for (; x < ccrr.to.x; x++) {
                auto& c = cells[y * gridSize.x + x];
                for (int32_t i = 0; i < c.len; ++i) {
                    auto& n = nodes[c[i]];
                    if (n.inResults) continue;
                    n.inResults = 1;
                    results.Emplace(c[i]);
                }
            }

            // last cell: check right AB
            if (x == ccrr.to.x) {
                auto& c = cells[y * gridSize.x + x];
                for (int32_t i = 0; i < c.len; ++i) {
                    auto& n = nodes[c[i]];
                    if (n.inResults) continue;
                    if (n.aabb.from.x < aabb_.to.x) {
                        n.inResults = 1;
                        results.Emplace(c[i]);
                    }
                }
            }
        }

        // last row: check AB
        if (y == ccrr.to.y) {

            // first cell: check left bottom AB
            x = ccrr.from.x;
            auto& c = cells[y * gridSize.x + x];
            for (int32_t i = 0; i < c.len; ++i) {
                auto& n = nodes[c[i]];
                if (n.inResults) continue;
                if (n.aabb.to.x > aabb_.from.x && n.aabb.from.y < aabb_.to.y) {
                    n.inResults = 1;
                    results.Emplace(c[i]);
                }
            }

            // middle cells: check bottom AB
            for (++x; x < ccrr.to.x; x++) {
                auto& c = cells[y * gridSize.x + x];
                for (int32_t i = 0; i < c.len; ++i) {
                    auto& n = nodes[c[i]];
                    if (n.inResults) continue;
                    if (n.aabb.from.y < aabb_.to.y) {
                        n.inResults = 1;
                        results.Emplace(c[i]);
                    }
                }
            }

            // last cell: check right bottom AB
            if (x == ccrr.to.x) {
                auto& c = cells[y * gridSize.x + x];
                for (int32_t i = 0; i < c.len; ++i) {
                    auto& n = nodes[c[i]];
                    if (n.inResults) continue;
                    if (n.aabb.from.x < aabb_.to.x && n.aabb.from.y < aabb_.to.y) {
                        n.inResults = 1;
                        results.Emplace(c[i]);
                    }
                }
            }
        }

        // clear flags
        if (except_ != -1) {
            nodes[except_].inResults = 0;
        }
        for (int32_t i = 0; i < results.len; ++i) {
            nodes[results[i]].inResults = 0;
        }
    }



	
    /**************************************************************************************************/
    // xx_affine.h
    /**************************************************************************************************/


	void SimpleAffineTransform::PosScaleAnchorSize(XY const& pos, XY const& scale, XY const& anchorSize) {
		a = scale.x;
		d = scale.y;
		tx = pos.x - scale.x * anchorSize.x;
		ty = pos.y - scale.y * anchorSize.y;
	}

	void SimpleAffineTransform::Identity() {
		a = 1;
		d = 1;
		tx = 0;
		ty = 0;
	}

	XY const& SimpleAffineTransform::Offset() const {
		return (XY&)tx;
	}

	XY const& SimpleAffineTransform::Scale() const {
		return (XY&)a;
	}

	XY SimpleAffineTransform::operator()(XY const& point) const {
		return { (float)((double)a * point.x + tx), (float)((double)d * point.y + ty) };
	}

	SimpleAffineTransform SimpleAffineTransform::MakeConcat(SimpleAffineTransform const& t2) const {
		auto& t1 = *this;
		return { t1.a * t2.a, t1.d * t2.d, t1.tx * t2.a + t2.tx, t1.ty * t2.d + t2.ty };
	}

	SimpleAffineTransform SimpleAffineTransform::MakeInvert() const {
		auto& t = *this;
		auto determinant = 1 / (t.a * t.d);
		return { determinant * t.d, determinant * t.a, determinant * (-t.d * t.tx), determinant * (-t.a * t.ty) };
	}

	SimpleAffineTransform SimpleAffineTransform::MakeIdentity() {
		return { 1.0, 1.0, 0.0, 0.0 };
	}

	SimpleAffineTransform SimpleAffineTransform::MakePosScaleAnchorSize(XY const& pos, XY const& scale, XY const& anchorSize) {
		SimpleAffineTransform t;
		t.PosScaleAnchorSize(pos, scale, anchorSize);
		return t;
	}



    /**************************************************************************************************/
    // xx_rnd.h
    /**************************************************************************************************/

	void Rnd::SetSeed(uint64_t seed) {
		auto calc = [&]()->uint64_t {
			auto z = (seed += 0x9e3779b97f4a7c15);
			z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
			z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
			return z ^ (z >> 31);
			};
		auto v = calc();
		memcpy(&state[0], &v, 8);
		v = calc();
		memcpy(&state[2], &v, 8);
	}

	uint32_t Rnd::Get() {
		auto rotl = [](uint32_t x, int s)->uint32_t {
			return (x << s) | (x >> (32 - s));
			};
		auto result = rotl(state[1] * 5, 7) * 9;
		auto t = state[1] << 9;
		state[2] ^= state[0];
		state[3] ^= state[1];
		state[1] ^= state[2];
		state[0] ^= state[3];
		state[2] ^= t;
		state[3] = rotl(state[3], 11);
		return result;
	}

	void Rnd::NextBytes(void* buf, size_t len) {
		uint32_t v{};
		size_t i{};
		auto e = len & (std::numeric_limits<size_t>().max() - 3);
		for (; i < e; i += 4) {
			v = Get();
			memcpy((char*)buf + i, &v, 4);
		}
		if (i < len) {
			v = Get();
			memcpy((char*)buf + i, &v, len - i);
		}
	}

	std::string Rnd::NextWord(size_t siz, std::string_view chars) {
		assert(chars.size() < 256);
		if (!siz) {
			siz = Next(2, 10);
		}
		std::string s;
		s.resize(siz);
		NextBytes(s.data(), siz);
		for (auto& c : s) {
			c = chars[c % chars.size()];
		}
		return s;
	}



    /**************************************************************************************************/
    // xx_gl.h
    /**************************************************************************************************/

#ifndef NDEBUG
    void CheckGLErrorAt(const char* file, int line, const char* func) {
        if (auto e = glGetError(); e != GL_NO_ERROR) {
            printf("glGetError() == %d file = %s line = %d\n", e, file, line);
            throw e;
        }
    }
#endif

    GLuint GLTexture::MakeTex() {
        GLuint id{};
        glGenTextures(1, &id);
        SetTexParm(id, GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
        return id;
    }

    // filter:  GL_NEAREST  GL_LINEAR    wraper:  GL_CLAMP_TO_EDGE   GL_REPEAT
    void GLTexture::SetTexParm(GLuint id_, GLuint minFilter_, GLuint magFilter_, GLuint sWraper_, GLuint tWraper_) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, id_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, sWraper_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, tWraper_);
    }

    void GLTexture::Make(XY size_, bool hasAlpha_) {
        assert(id == -1);
        id = MakeTex();
        auto c = hasAlpha_ ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, c, size_.x, size_.y, 0, c, GL_UNSIGNED_BYTE, {});
        size = size_;
    }

    void GLTexture::SetParm(GLuint minFilter_, GLuint magFilter_, GLuint sWraper_, GLuint tWraper_) {
        SetTexParm(id, minFilter_, magFilter_, sWraper_, tWraper_);
    }

    void GLTexture::SetParm(GLuint minmagFilter_, GLuint stWraper_) {
        SetTexParm(id, minmagFilter_, minmagFilter_, stWraper_, stWraper_);
    }

    void GLTexture::TryGenerateMipmap() {
        if (size.x == size.y && Round2n((size_t)size.x) == (size_t)size.x) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, id);
            glGenerateMipmap(GL_TEXTURE_2D);
            CheckGLError();
        }
    }

    GLShader LoadGLShader(GLenum type, std::initializer_list<std::string_view>&& codes_) {
        assert(codes_.size() && (type == GL_VERTEX_SHADER || type == GL_FRAGMENT_SHADER));
        auto&& shader = glCreateShader(type);
        assert(shader);
        std::vector<GLchar const*> codes;
        codes.resize(codes_.size());
        std::vector<GLint> codeLens;
        codeLens.resize(codes_.size());
        auto ss = codes_.begin();
        for (size_t i = 0; i < codes.size(); ++i) {
            codes[i] = (GLchar const*)ss[i].data();
            codeLens[i] = (GLint)ss[i].size();
        }
        glShaderSource(shader, (GLsizei)codes_.size(), codes.data(), codeLens.data());
        glCompileShader(shader);
        GLint r = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &r);
        if (!r) {
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &r);	// fill txt len into r
            std::string s;
            if (r) {
                s.resize(r);
                glGetShaderInfoLog(shader, r, nullptr, s.data());	// copy txt to s
            }
            printf("glCompileShader failed: err msg = %s", s.c_str());
            throw s;
        }
        return GLShader(shader);
    }

    GLShader LoadGLVertexShader(std::initializer_list<std::string_view>&& codes_) {
        return LoadGLShader(GL_VERTEX_SHADER, std::move(codes_));
    }

    GLShader LoadGLFragmentShader(std::initializer_list<std::string_view>&& codes_) {
        return LoadGLShader(GL_FRAGMENT_SHADER, std::move(codes_));
    }

    GLProgram LinkGLProgram(GLuint vs, GLuint fs) {
        auto program = glCreateProgram();
        assert(program);
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);
        GLint r = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &r);
        if (!r) {
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &r);
            std::string s;
            if (r) {
                s.resize(r);
                glGetProgramInfoLog(program, r, nullptr, s.data());
            }
            printf("glLinkProgram failed: err msg = %s", s.c_str());
            throw s;
        }
        return GLProgram(program);
    }


    // data's bytes len == w * h * sizeof(colorFormat)
    GLTexture LoadGLTexture(void* data, GLsizei w, GLsizei h, GLint colorFormat) {
        auto id = GLTexture::MakeTex();
        if (colorFormat == GL_RGBA) {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 8 - 4 * (w & 0x1));
        }
        glTexImage2D(GL_TEXTURE_2D, 0, colorFormat, w, h, 0, colorFormat, GL_UNSIGNED_BYTE, data);
        CheckGLError();
        return { id, {w, h} };
    }


    GLTexture LoadGLTexture(void* buf_, size_t len_) {
        assert(len_ >= 16);
        std::string_view buf((char*)buf_, len_);

        // png
        if (buf.starts_with("\x89\x50\x4e\x47\x0d\x0a\x1a\x0a"sv)) {
            int w, h, comp;
            if (auto image = stbi_load_from_memory((stbi_uc*)buf.data(), (int)buf.size(), &w, &h, &comp, 0)) {
                assert(comp == 3 || comp == 4);
                auto t = LoadGLTexture(image, w, h, comp == 3 ? GL_RGB : GL_RGBA);
                stbi_image_free(image);
                return t;
            }
        }

        // jpg
        else if (buf.starts_with("\xFF\xD8"sv)) {
            int w, h, comp;
            if (auto image = stbi_load_from_memory((stbi_uc*)buf.data(), (int)buf.size(), &w, &h, &comp, 0)) {
                assert(comp == 3 || comp == 4);
                auto t = LoadGLTexture(image, w, h, GL_RGB);
                stbi_image_free(image);
                return t;
            }
        }

        // pvr3 with Etc2_RGBA
        // todo: soft decompress for MACOS
        /* https://github.com/wolfpld/etcpak.git
        * BlockData.cpp / WritePvrHeader
    uint32_t* dst;
    *dst++ = 0x03525650;  // version				// PVR\x3
    *dst++ = 0;           // flags
    switch( type ) {      // pixelformat[0]
    case CodecType::Etc2_RGB:        *dst++ = 22;        break;
    case CodecType::Etc2_RGBA:        *dst++ = 23;        break;
    case CodecType::Etc2_R11:        *dst++ = 25;        break;
    case CodecType::Etc2_RG11:        *dst++ = 26;        break;
        ........
    }
    *dst++ = 0;           // pixelformat[1]
    *dst++ = 0;           // colourspace
    *dst++ = 0;           // channel type
    *dst++ = size.y;      // height
    *dst++ = size.x;      // width
    *dst++ = 1;           // depth
    *dst++ = 1;           // num surfs
    *dst++ = 1;           // num faces
    *dst++ = levels;      // mipmap count
    *dst++ = 0;           // metadata size

        */
        else if (buf.starts_with("PVR\x3"sv)) {
            auto p = (uint32_t*)buf.data();
            ++p;				// version
            assert(*p == 0);	// flags
            auto format = *++p;		// GL_COMPRESSED_RGB8_ETC2: 0x9274
            assert(format == 23);	// GL_COMPRESSED_RGBA8_ETC2_EAC: 0x9278
            p += 3;				// pixelformat[1], colourspace, channel type
            auto height = *++p;
            auto width = *++p;
            p += 3;				// depth, num surfs, num faces
            auto levels = *++p;
            ++p;				// metadata size
            ++p;
            //auto dataLen = buf.size() - ((char*)p - buf.data());
            auto dataLen = width * height;
            auto ptr = (char*)p;

            //glPixelStorei(GL_UNPACK_ALIGNMENT, 8 - 4 * (width & 0x1));
            auto id = GLTexture::MakeTex();
            for (uint32_t i = 0; i < levels; ++i) {
                glCompressedTexImage2D(GL_TEXTURE_2D, (GLint)i, 0x9278, (GLsizei)width, (GLsizei)height, 0, (GLsizei)dataLen, ptr);
                CheckGLError();
                ptr += dataLen;
                assert(ptr <= buf.data() + buf.size());
                dataLen /= 4;
                width /= 2;
                height /= 2;
                if (width < 4 || height < 4) break;
            }
            //glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, 0);
            CheckGLError();
            return { id, {width, height}/*, fullPath*/ };
        }

        // ...

        throw __LINE__;
        return {};
    }

    GLTexture LoadGLTexture(Span d) {
        return LoadGLTexture(d.buf, d.len);
    }

    GLFrameBuffer MakeGLFrameBuffer() {
        GLuint f{};
        glGenFramebuffers(1, &f);
        //glBindFramebuffer(GL_FRAMEBUFFER, f);
        return GLFrameBuffer(f);
    }

    void BindGLFrameBufferTexture(GLuint f, GLuint t) {
        glBindFramebuffer(GL_FRAMEBUFFER, f);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t, 0);
        assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    }

    void UnbindGLFrameBuffer() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // only support GL_RGBA, GL_UNSIGNED_BYTE
    void GLFrameBufferSaveTo(Data& tar, GLint x, GLint y, GLsizei width, GLsizei height) {
        tar.Resize(width * height * 4);
        glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, tar.buf);
    }



    /**************************************************************************************************/
    // xx_framebuffer.h
    /**************************************************************************************************/

    // need ogl frame env
    FrameBuffer& FrameBuffer::Init() {
        assert(fb == (GLuint)-1);
        fb = MakeGLFrameBuffer();
        return *this;
    }

    void FrameBuffer::Begin(Shared<GLTexture>& t, std::optional<RGBA8> const& c) {
        assert(t);
        auto& g = *GameBase::instance;
        g.ShaderEnd();
        bakWndSiz = g.windowSize;
        bakBlend = g.blend;
        bakTexSiz = t->size;
        g.SetWindowSize(t->size);
        g.flipY = -1;
        BindGLFrameBufferTexture(fb, *t);
        g.GLViewport();
        if (c.has_value()) {
            g.GLClear(c.value());
        }
        g.GLBlendFunc(g.blendDefault);
    }

    void FrameBuffer::End(Data* store) {
        auto& g = *GameBase::instance;
        g.ShaderEnd();
        if (store) {
            GLFrameBufferSaveTo(*store, 0, 0, bakTexSiz.x, bakTexSiz.y);
        }
        UnbindGLFrameBuffer();
        g.SetWindowSize(bakWndSiz);
        g.flipY = 1;
        g.GLViewport();
        g.GLBlendFunc(bakBlend);
    }




    /**************************************************************************************************/
    // xx_rectpacker.h
    /**************************************************************************************************/

    int32_t RectPacker::Pack(XY texSize_, XY padding_) {
        assert(tfs.len);
        auto siz = texSize_.As<int32_t>();
        assert(siz.x > 0 && siz.y > 0);

        rects.Clear();
        rects.Reserve(tfs.len);
        auto grow = (padding_ * 2).As<int32_t>();
        for (int32_t i = 0; i < tfs.len; ++i) {
            auto& tf = tfs[i];
            auto& rect = rects.Emplace();
            rect.id = i;
            rect.w = tf->uvRect.w + grow.x;
            rect.h = tf->uvRect.h + grow.y;
        }

        stbrp_context c;
        nodes.Resize(siz.x);
        stbrp_init_target(&c, siz.x, siz.y, nodes.buf, nodes.len);
        auto r = stbrp_pack_rects(&c, rects.buf, rects.len);
        if (r == 0) return __LINE__;

        Shared<GLTexture> t;
        t.Emplace()->Make(texSize_);
        FrameBuffer{}.Init().DrawTo(t, {}, [&] {
            XY basePos{ -texSize_.x / 2, -texSize_.y / 2 };
            for (auto& o : rects) {
                auto& tf = *tfs[o.id];
                o.x += padding_.x;
                o.y += padding_.y;
                auto pos = basePos + XY{ o.x, o.y };
                GameBase::instance->Quad().Draw(*tf.tex, tf.uvRect, pos, 0);
            }
            });
        for (auto& o : rects) {
            auto& tf = *tfs[o.id];
            tf.tex = t;
            tf.uvRect.x = uint16_t(o.x);
            tf.uvRect.y = uint16_t(texSize_.y - o.y - tf.uvRect.h);
        }

        return 0;
    }

    int32_t RectPacker::AutoPack(int32_t minPackSize_, XY padding_) {
    LabRetry:
        if (auto r = Pack(minPackSize_); r) {
            minPackSize_ *= 2;
            assert(minPackSize_ <= 16384);
            goto LabRetry;
        }
        tfs[0]->tex->TryGenerateMipmap();
        return 0;
    }




    /**************************************************************************************************/
    // xx_node.h
    /**************************************************************************************************/

	XY Node::ToLocalPos(XY worldPos_) {
		return trans().MakeInvert()(worldPos_);
	}
	XY Node::ToParentLocalPos(XY worldPos_) {
		return parent->trans().MakeInvert()(worldPos_);
	}

	XY Node::GetScaledSize() const {
		return scale * size;
	}

	void Node::FillTrans() {
		if (parent) {
			trans() = SimpleAffineTransform::MakePosScaleAnchorSize(position, scale, anchor * size).MakeConcat(parent->trans());
		}
		else {
			trans().PosScaleAnchorSize(position, scale, anchor * size);
		}

		worldMaxXY = trans()(size);
		worldSize = worldMaxXY - worldMinXY;

		TransUpdate();
	}

	bool Node::IsVisible() const {
		if (!visible) return false;
		if (scissor && !IsIntersect_BoxBoxF(worldMinXY, worldMaxXY, scissor->worldMinXY, scissor->worldMaxXY)) return false;
		if (inParentArea && parent) return IsIntersect_BoxBoxF(worldMinXY, worldMaxXY, parent->worldMinXY, parent->worldMaxXY);
		return IsIntersect_BoxBoxF(worldMinXY, worldMaxXY, GameBase::instance->worldMinXY, GameBase::instance->worldMaxXY);
	}

	bool Node::PosInArea(XY pos_) const {
		if (scissor && !IsIntersect_BoxPointF(scissor->worldMinXY, scissor->worldMaxXY, pos_)) return false;
		return IsIntersect_BoxPointF(worldMinXY, worldMaxXY, pos_);
	}

	bool Node::PosInScissor(XY pos_) const {
		if (!scissor) return true;
		return IsIntersect_BoxPointF(scissor->worldMinXY, scissor->worldMaxXY, pos_);
	}

	// for update
	void Node::FillTransRecursive() {
		FillTrans();
		for (auto& c : children) {
			c->FillTransRecursive();
		}
	};

	void Node::SetVisibleRecursive(bool visible_) {
		visible = visible_;
		for (auto& c : children) {
			c->SetVisibleRecursive(visible_);
		}
	}

	void Node::SetEnabledRecursive(bool enabled_) {
		enabled = enabled_;
		for (auto& c : children) {
			c->SetEnabledRecursive(enabled_);
		}
	}

	void Node::SetAlphaRecursive(float alpha_) {
		alpha = alpha_;
		for (auto& c : children) {
			c->SetAlphaRecursive(alpha_);
		}
	}

	void Node::SetScissorRecursive(Weak<Node> scissor_) {
		scissor = scissor_;
		for (auto& c : children) {
			c->SetScissorRecursive(scissor_);
		}
	}

	Node& Node::Init(int32_t z_, XY position_, XY anchor_, XY scale_, XY size_) {
		return InitDerived<Node>(z_, position_, anchor_, scale_, size_);
	}

	// for ui root node only
	Node& Node::InitRoot(XY scale_) {
		return Init(0, 0, 0, scale_);
	}

	void Node::SetToUIHandler(bool handle) {
		auto& h = GameBase::instance->uiHandler;
		if (handle) {
			h = WeakFromThis((MouseEventHandlerNode*)this);
		}
		else {
			if ((Node*)h.TryGetPointer() == this) {
				h.Reset();
			}
		}
	}

	void Node::SwapRemove() {
		assert(parent);
		assert(parent->children[indexAtParentChildren].pointer == this);
		auto i = parent->children.Back()->indexAtParentChildren = indexAtParentChildren;
		indexAtParentChildren = -1;
		auto p = parent.GetPointer();
		parent.Reset();
		p->children.SwapRemoveAt(i);
	}

	void Node::Clear() {
		for (auto i = children.len - 1; i >= 0; --i) {
			children[i]->SwapRemove();
		}
	}

	void Node::FindTopESCHandler(Node*& out, int32_t& minZ) {
		if (escHandler && z > minZ) {
			out = this;
			minZ = z;
		}
		for (auto& c : children) {
			c->FindTopESCHandler(out, minZ);
		}
	}

	// recursive find TOP ESC handler
	// null: not found
	Node* Node::FindTopESCHandler() {
		Node* n{};
		auto minZ = std::numeric_limits<int32_t>::min();
		FindTopESCHandler(n, minZ);
		return n;
	}

	void Node::HandleESC() {
		SwapRemove();
	}

	void Node::TryRegisterAutoUpdate() {
		auto& c = GameBase::instance->uiAutoUpdates;
		auto w = WeakFromThis(this);
		for (int32_t i = 0, e = c.len; i < e; ++i) {
			if (c[i].h == w.h) return;
		}
		c.Emplace(std::move(w));
	}


	void Node::Resize(XY scale_) {
		scale = scale_;
		FillTransRecursive();
	}

	void Node::Resize(XY position_, XY scale_) {
		position = position_;
		scale = scale_;
		FillTransRecursive();
	}

	Node* Node::FindFirstTypeId(int32_t typeId_) {
		if (this->typeId == typeId_) return this;
		for (int32_t i = 0; i < children.len; ++i) {
			if (children[i]->typeId == typeId_) {
				return children[i].pointer;
			}
		}
		return {};
	}





	bool MouseEventHandlerNode::MousePosInArea() const {
		return PosInArea(GameBase::instance->mousePos);
	}

	void MouseEventHandlerNode::TransUpdate() {
		auto& g = GameBase::instance->uiGrid;
		auto w2 = g.worldSize * 0.5f;
		FromTo<XY> aabb{ worldMinXY + w2, worldMaxXY + w2 };

		if (g.TryLimitAABB(aabb)) {
			if (indexAtUiGrid != -1) {
				g.Update(indexAtUiGrid, aabb);
			}
			else {
				indexAtUiGrid = g.Add(aabb, this);
			}
		}
		else {
			if (indexAtUiGrid != -1) {
				g.Remove(indexAtUiGrid);
				indexAtUiGrid = -1;
			}
		}
	}

	MouseEventHandlerNode::~MouseEventHandlerNode() {
		if (indexAtUiGrid != -1) {
			GameBase::instance->uiGrid.Remove(indexAtUiGrid);
			indexAtUiGrid = -1;
		}
	}




    /**************************************************************************************************/
    // xx_bmfont.h
    /**************************************************************************************************/

    // load binary .fnt & .pngs from file
    // return 0: success
    int32_t BMFont::Init(std::string_view fn) {
        auto p = GameBase::instance->GetFullPath(fn);
        auto d = ReadAllBytes_sv(p);
        if (d.len < 4) return __LINE__; // throw std::logic_error(ToString("BMFont file's size is too small. fn = ", p));
        if (std::string_view((char*)d.buf, 3) != "BMF"sv) return __LINE__; // throw std::logic_error(ToString("bad BMFont format. fn = ", p));
        if (d[3] != 3) return __LINE__; // throw std::logic_error(ToString("BMFont only support version 3. fn = ", p));
        if (auto r = Init(d.buf, d.len, p); r) return r;
        return 0;
    }

    // load font & texture from memory
    // tex: for easy load font texture from memory
    int32_t BMFont::Init(uint8_t const* buf_, size_t len_, std::string fullPath_, bool autoLoadTexture) {
        fullPath.clear();
        Data_r d{ buf_, len_ };

        // cleanup for logic safety
        memset(charArray.data(), 0, sizeof(charArray));
        charMap.clear();
        kernings.clear();
        if (autoLoadTexture) {
            texs.Clear();
        }
        paddingLeft = paddingTop = paddingRight = paddingBottom = fontSize = lineHeight = 0;

        List<std::string> texFNs;
        uint16_t pages{};

        (void)d.ReadJump(4);  // skip BMF\x3
        while (d.HasLeft()) {
            uint8_t blockId;
            if (auto r = d.ReadFixed(blockId)) return __LINE__; // throw std::logic_error(ToString("BMFont read blockId error. r = ", r, ". fn = ", p));
            uint32_t blockSize;
            if (auto r = d.ReadFixed(blockSize)) return __LINE__; // throw std::logic_error(ToString("BMFont read blockSize error. r = ", r, ". fn = ", p));
            if (d.offset + blockSize > d.len) return __LINE__; // throw std::logic_error(ToString("BMFont bad blockSize = ", blockSize, ". fn = ", p));

            Data_r dr(d.buf + d.offset, blockSize);
            if (blockId == 1) {
                /*
                    fontSize       2   int      0
                    bitField       1   bits     2  bit 0: smooth, bit 1: unicode, bit 2: italic, bit 3: bold, bit 4: fixedHeight, bits 5-7: reserved
                    charSet        1   uint     3
                    stretchH       2   uint     4
                    aa             1   uint     6
                    paddingUp      1   uint     7
                    paddingRight   1   uint     8
                    paddingDown    1   uint     9
                    paddingLeft    1   uint     10
                    spacingHoriz   1   uint     11
                    spacingVert    1   uint     12
                    outline        1   uint     13 added with version 2
                    fontName       n+1 string   14 null terminated string with length n
                    */

                if (auto r = dr.ReadFixed(fontSize)) return __LINE__; // throw std::logic_error(ToString("BMFont read fontSize error. r = ", r, ". fn = ", p));
                if (auto r = dr.ReadJump(5)) return __LINE__; // throw std::logic_error(ToString("BMFont read jump 5 error. r = ", r, ". fn = ", p));
                if (auto r = dr.ReadFixed(paddingTop)) return __LINE__; // throw std::logic_error(ToString("BMFont read paddingTop error. r = ", r, ". fn = ", p));
                if (auto r = dr.ReadFixed(paddingRight)) return __LINE__; // throw std::logic_error(ToString("BMFont read paddingRight error. r = ", r, ". fn = ", p));
                if (auto r = dr.ReadFixed(paddingBottom)) return __LINE__; // throw std::logic_error(ToString("BMFont read paddingBottom error. r = ", r, ". fn = ", p));
                if (auto r = dr.ReadFixed(paddingLeft)) return __LINE__; // throw std::logic_error(ToString("BMFont read paddingLeft error. r = ", r, ". fn = ", p));
                fontSize = std::abs(fontSize);  // maybe negative

            }
            else if (blockId == 2) {
                /*
                    lineHeight 2   uint    0
                    base       2   uint    2
                    scaleW     2   uint    4
                    scaleH     2   uint    6
                    pages      2   uint    8
                    bitField   1   bits    10  bits 0-6: reserved, bit 7: packed
                    alphaChnl  1   uint    11
                    redChnl    1   uint    12
                    greenChnl  1   uint    13
                    blueChnl   1   uint    14
                    */

                if (auto r = dr.ReadFixed(lineHeight)) return __LINE__; // throw std::logic_error(ToString("BMFont read lineHeight error. r = ", r, ". fn = ", p));
                if (auto r = dr.ReadJump(6)) return __LINE__; // throw std::logic_error(ToString("BMFont read jump 6 error. r = ", r, ". fn = ", p));

                if (auto r = dr.ReadFixed(pages)) return __LINE__; // throw std::logic_error(ToString("BMFont read pages error. r = ", r, ". fn = ", p));
                if (pages < 1) return __LINE__; // throw std::logic_error(ToString("BMFont pages < 1. fn = ", p));

            }
            else if (blockId == 3) {
                /*
                    pageNames 	p*(n+1) 	strings 	0 	p null terminated strings, each with length n
                    */

                for (int i = 0; i < (int)pages; ++i) {
                    auto&& texFN = texFNs.Emplace();
                    if (auto r = dr.ReadCStr(texFN)) return __LINE__; // throw std::logic_error(ToString("BMFont read pageNames[", i, "] error. r = ", r, ". fn = ", p));
                }

                // attach prefix dir name
                if (auto i = fullPath_.find_last_of("/"); i != fullPath_.npos) {
                    for (auto& s : texFNs) {
                        s = fullPath_.substr(0, i + 1) + s;
                    }
                }

            }
            else if (blockId == 4) {
                /*
                    id         4   uint    0+c*20  These fields are repeated until all characters have been described
                    x          2   uint    4+c*20
                    y          2   uint    6+c*20
                    width      2   uint    8+c*20
                    height     2   uint    10+c*20
                    xoffset    2   int     12+c*20
                    yoffset    2   int     14+c*20
                    xadvance   2   int     16+c*20
                    page       1   uint    18+c*20
                    chnl       1   uint    19+c*20
                    */

                for (uint32_t count = blockSize / 20, i = 0; i < count; i++) {
                    uint32_t id;
                    if (auto r = dr.ReadFixed(id)) return __LINE__; // throw std::logic_error(ToString("BMFont read id error. r = ", r, ". fn = ", p));

                    auto&& result = charMap.emplace(id, BMFontChar{});
                    if (!result.second) return __LINE__; // throw std::logic_error(ToString("BMFont insert to charRectMap error. BMFontChar id = ", id, ". fn = ", p));

                    auto& c = result.first->second;
                    if (auto r = dr.ReadFixed(c.x)) return __LINE__; // throw std::logic_error(ToString("BMFont read c.x error. r = ", r, ". fn = ", p));
                    if (auto r = dr.ReadFixed(c.y)) return __LINE__; // throw std::logic_error(ToString("BMFont read c.y error. r = ", r, ". fn = ", p));
                    if (auto r = dr.ReadFixed(c.width)) return __LINE__; // throw std::logic_error(ToString("BMFont read c.width error. r = ", r, ". fn = ", p));
                    if (auto r = dr.ReadFixed(c.height)) return __LINE__; // throw std::logic_error(ToString("BMFont read c.height error. r = ", r, ". fn = ", p));
                    if (auto r = dr.ReadFixed(c.xoffset)) return __LINE__; // throw std::logic_error(ToString("BMFont read c.xoffset error. r = ", r, ". fn = ", p));
                    if (auto r = dr.ReadFixed(c.yoffset)) return __LINE__; // throw std::logic_error(ToString("BMFont read c.yoffset error. r = ", r, ". fn = ", p));
                    if (auto r = dr.ReadFixed(c.xadvance)) return __LINE__; // throw std::logic_error(ToString("BMFont read c.xadvance error. r = ", r, ". fn = ", p));
                    if (auto r = dr.ReadFixed(c.page)) return __LINE__; // throw std::logic_error(ToString("BMFont read c.page error. r = ", r, ". fn = ", p));
                    if (c.page >= pages) return __LINE__; // throw std::logic_error(ToString("BMFont c.page out of range. c.page = ", c.page, ", pages = ", pages, ". fn = ", p));
                    if (auto r = dr.ReadFixed(c.chnl)) return __LINE__; // throw std::logic_error(ToString("BMFont read c.chnl error. r = ", r, ". fn = ", p));

                    if (id < 256) {
                        charArray[id] = c;
                    }
                }
            }
            else if (blockId == 5) {
                /*
                    first  4   uint    0+c*10 	These fields are repeated until all kerning pairs have been described
                    second 4   uint    4+c*10
                    amount 2   int     8+c*10
                    */

                uint32_t first, second;
                int16_t amount;
                for (uint32_t count = blockSize / 10, i = 0; i < count; i++) {
                    if (auto r = dr.ReadFixed(first)) return __LINE__; // throw std::logic_error(ToString("BMFont read first error. r = ", r, ". fn = ", p));
                    if (auto r = dr.ReadFixed(second)) return __LINE__; // throw std::logic_error(ToString("BMFont read second error. r = ", r, ". fn = ", p));
                    if (auto r = dr.ReadFixed(amount)) return __LINE__; // throw std::logic_error(ToString("BMFont read amount error. r = ", r, ". fn = ", p));

                    uint64_t key = ((uint64_t)first << 32) | ((uint64_t)second & 0xffffffffll);
                    kernings[key] = amount;
                }
            }

            if (auto r = d.ReadJump(blockSize)) return __LINE__; // throw std::logic_error(ToString("BMFont read jump blockSize error. blockSize = ", blockSize, ". r = ", r, ". fn = ", p));
        }

        // load textures
        if (autoLoadTexture) {
            for (auto&& f : texFNs) {
                texs.Emplace(GameBase::instance->LoadTexture(f));
            }
        }

        // fill texId
        for (auto& c : charArray) {
            c.texId = texs[c.page]->id;
        }
        for (auto& [k, v] : charMap) {
            v.texId = texs[v.page]->id;
        }

        // store display info when success
        fullPath = std::move(fullPath_);
        return 0;
    }

    // texture index: page
    BMFontChar const* BMFont::Get(char32_t charId) const {
        assert(charId >= 0);
        if (charId < 256) {
            auto& c = charArray[charId];
            if ((uint64_t&)c) {
                return &c;
            }
        }
        else {
            if (auto iter = charMap.find(charId); iter != charMap.end()) {
                return &iter->second;
            }
        }
        return nullptr;
    }





    /**************************************************************************************************/
    // xx_input.h
    /**************************************************************************************************/

    BtnState::operator bool() const {
        return pressed;
    }

    void BtnState::Press() {
        pressed = true;
        nextActiveTime = {};
    }

    void BtnState::Release() {
        pressed = false;
        nextActiveTime = {};
    }

    bool BtnState::Once() {
        if (pressed) {
            Release();
            return true;
        }
        return false;
    }

    bool BtnState::operator()(float interval_) {
        if (pressed) {
            if (auto t = float(*globalTime); nextActiveTime <= t) {
                nextActiveTime = t + interval_;
                return true;
            }
        }
        return false;
    }





    void JoyState::Init(double* globalTime_) {
        for (auto& o : btns) {
            o.globalTime = globalTime_;
        }
    }

    void JoyState::ClearValues() {
        for (auto& btn : btns) {
            btn.pressed = 0;
            //btn.lastPressedTime = 0;
        }
        memset(&axes, 0, sizeof(float) * GLFW_GAMEPAD_AXIS_LAST - 1);
        axes[GLFW_GAMEPAD_AXIS_LAST - 1] = -1.f;
        axes[GLFW_GAMEPAD_AXIS_LAST] = -1.f;
    }
    void JoyState::Cleanup() {
        jid = -1;
        name.clear();
        ClearValues();
    }





    /**************************************************************************************************/
    // xx_camera.h
    /**************************************************************************************************/

	void Camera::Init(float baseScale_, float logicScale_, XY original_) {
		baseScale = baseScale_;
		logicScale = logicScale_;
		scale = logicScale * baseScale;
		_1_scale = 1.f / scale;
		original = original_;
	}

	void Camera::SetOriginal(XY original_) {
		original = original_;
	}

	void Camera::SetBaseScale(float baseScale_) {
		baseScale = baseScale_;
		scale = logicScale * baseScale;
		_1_scale = 1.f / scale;
	}

	void Camera::SetLogicScale(float logicScale_) {
		logicScale = logicScale_;
		scale = logicScale * baseScale;
		_1_scale = 1.f / scale;
	}

	XY Camera::ToGLPos(XY logicPos_) const {
		return (logicPos_ - original - offset).FlipY() * scale;
	}

	XY Camera::ToLogicPos(XY glPos_) const {
		return { glPos_.x * _1_scale + original.x + offset.x, -glPos_.y * _1_scale + original.y + offset.y };
	}

}

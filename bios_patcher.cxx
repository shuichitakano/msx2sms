/* -*- mode:C++; -*-
 *
 * bios_patcher.cxx
 *
 * author(s) : Shuichi TAKANO
 * since 2016/09/10(Sat) 14:02:37
 */

#include <vector>
#include <stdint.h>
#include <fstream>
#include <stdio.h>
#include <algorithm>
#include <assert.h>
#include <set>

////////////////////////////////////////////////////////////////////////

bool getOption(char **argv, const char *f1, const char *f2, bool &flag)
{
    const char *s = argv[0];
    assert(f1 || f2);

    if ((f1 && strcmp(s, f1) == 0) ||
        (f2 && strcmp(s, f2) == 0))
    {
        flag = true;
        return true;
    }
    return false;
}

template <class T, class Func>
bool getOption(int &argc, char **&argv,
               const char *f1, const char *f2, Func func, T &ret)
{
    const char *s = argv[0];
    assert(f1 || f2);
    size_t l1 = f1 ? strlen(f1) : 0;
    size_t l2 = f2 ? strlen(f2) : 0;
    size_t l = 0;
    if (l1 && strncmp(s, f1, l1) == 0)
        l = l1;
    else if (l2 && strcmp(s, f2) == 0)
        l = l2;

    if (l)
    {
        if (s[l] == 0)
        {
            --argc;
            ++argv;
            if (argc)
                ret = func(argv[0]);
            else
                return false;
        }
        else
            ret = func(s + l);
        return true;
    }
    return false;
}

const char *getConstStr(const char *str)
{
    return str;
}

template <class T>
struct StringBackInserter
{
    T &strs_;

    StringBackInserter(T &strs) : strs_(strs) {}

    void operator=(const char *n) const
    {
        strs_.push_back(n);
    }
};

template <class T>
const StringBackInserter<T> stringBackInserter(T &strs)
{
    return StringBackInserter<T>(strs);
}

///////////////////////////////////////////////////////////////////////////////

const char *output_ = nullptr;
const char *rom_ = nullptr;
const char *bios_ = nullptr;
const char *biosPatch_ = nullptr;
const char *mapper_ = nullptr;

std::vector<std::string> addPages_;
std::vector<std::string> romPatches_;
std::vector<std::string> startKeys_;

bool reversePSGToneOrder_ = false;

bool analyzeCommandLine(int argc, char **argv)
{
    --argc;
    ++argv;
    while (argc > 0)
    {
        if (
            !getOption(argc, argv, "-o", "--output", getConstStr, output_) &&
            !getOption(argc, argv, "-r", "--rom", getConstStr, rom_) &&
            !getOption(argc, argv, "-b", "--biosrom", getConstStr, bios_) &&
            !getOption(argc, argv, "-p", "--biospatch", getConstStr, biosPatch_) &&
            !getOption(argc, argv, "-m", "--mapper", getConstStr, mapper_) &&
            !getOption(argv, "-t", "--rev_tone_order", reversePSGToneOrder_) &&
            !getOption(argc, argv, nullptr, "--pagepair", getConstStr, stringBackInserter(addPages_)) &&
            !getOption(argc, argv, nullptr, "--patch", getConstStr, stringBackInserter(romPatches_)) &&
            !getOption(argc, argv, nullptr, "--stkey", getConstStr, stringBackInserter(startKeys_)) &&
            true)
        {
            if (argc == 0)
                return false;

            const char *s = argv[0];
            if (s[0] == '-' || s[0] == '/')
                return false;

            /* 普通 */
            //            if (!input_)
            //                input_ = s;
            //            else if (!output_)
            //                output_ = s;
            //            else
            return false;
        }
        --argc;
        ++argv;
    }
    return true;
}

void printUsage(const char *cmd)
{
    printf("%s\n", cmd);
    printf("options :\n");
    printf(" -o --output          : output filename.sms\n");
    printf(" -r --rom             : input MSX rom filename\n");
    printf(" -b --biosrom         : MSX bios rom\n");
    printf(" -p --biospatch       : bios patch bin\n");
    printf(" -m --mapper          : mapper type\n");
    printf(" -t --rev_tone_order  : reverse PSG tone set order\n");
    printf("    --pagepair [xxyy] : add mapper page pair (2hex)\n");
    printf("    --patch [addr:vv] : add rom patch\n");
    printf("    --stkey [name]    : add start button key\n");
}

//////////////////////////////////////////////////////////////////////

void load(std::vector<uint8_t> &dst, const char *filename)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp)
    {
        printf("load error %s\n", filename);
        exit(-1);
    }

    uint8_t buf[1000];
    while (1)
    {
        auto r = fread(buf, 1, sizeof(buf), fp);
        if (r == 0)
            break;
        dst.insert(dst.end(), buf, buf + r);
    }

    printf("%zd bytes loaded. %s.\n",
           dst.size(), filename);

    fclose(fp);
}

void save(const std::vector<uint8_t> &src, const char *filename)
{
    FILE *fp = fopen(filename, "wb");
    if (!fp)
    {
        printf("save error %s\n", filename);
        exit(-1);
    }

    fwrite(src.data(), 1, src.size(), fp);
    fclose(fp);

    printf("%s saved.\n", filename);
}

template <class T1, class T2>
void replaceArea(T1 &dst, const T2 &src,
                 int startOfs, int endOfs)
{
    memcpy(dst.data() + startOfs,
           src.data() + startOfs, endOfs - startOfs);
}

template <class T>
void replacePattern2(T &data, int p0, int p1, int r0, int r1)
{
    int size = (int)data.size() - 1;
    //    int size = 0x8000 - 1;	// biosだけ。手抜き
    for (int i = 0; i < size; ++i)
    {
        if (data[i + 0] == p0 &&
            data[i + 1] == p1)
        {
            data[i + 0] = r0;
            data[i + 1] = r1;
        }
    }
}

template <class T>
void replacePattern3(T &data,
                     int p0, int p1, int p2,
                     int r0, int r1, int r2)
{
    int size = (int)data.size() - 2;
    //    int size = 0x8000 - 2;	// biosだけ。手抜き
    for (int i = 0; i < size; ++i)
    {
        if (data[i + 0] == p0 &&
            data[i + 1] == p1 &&
            data[i + 2] == p2)
        {
            data[i + 0] = r0;
            data[i + 1] = r1;
            data[i + 2] = r2;
        }
    }
}

template <class T>
void writeBytes(T &data, int ofs, size_t size, const uint8_t *buf)
{
    std::copy(buf, buf + size, data.begin() + ofs);
}

int maskToCount(int m)
{
    if (m < 0)
        return -1;
    return m;
}

int rotL(int v)
{
    if (v < 0)
        return -1;
    return ((v << 1) & 255) | ((v >> 7) & 1);
}

int add(int v, int i)
{
    if (v < 0)
        return -1;
    return (v + i) & 255;
}

struct MEGAROMAnalyzer
{
    struct PagePair
    {
        int pages_[2] = {-1, -1};

        bool isValid() const
        {
            return pages_[0] >= 0 || pages_[1] >= 0;
        }

        friend bool operator<(const PagePair &a, const PagePair &b)
        {
            if (a.pages_[0] != b.pages_[0])
                return a.pages_[0] < b.pages_[0];
            return a.pages_[1] < b.pages_[1];
        }
    };

    struct BankInfo
    {
        std::set<int> certainly_;
        std::set<int> perhaps_;
    };

    enum MapperType
    {
        MAPPER_ASCII8KB,
        MAPPER_ASCII16KB,
        MAPPER_KONAMI8KB,
        MAPPER_KONAMI8KB_SCC,
    };

    int getBankSize(MapperType mapper)
    {
        switch (mapper)
        {
        default:
        case MAPPER_ASCII8KB:
            return 8 * 1024;

        case MAPPER_ASCII16KB:
            return 16 * 1024;

        case MAPPER_KONAMI8KB:
            return 8 * 1024;

        case MAPPER_KONAMI8KB_SCC:
            return 8 * 1024;
        }
        return 0;
    }

    struct Record
    {
        struct Entry
        {
            int ofs_ = 0;
            int bank_ = 0;
            int page_ = -1;         // 確実なページ番号
            int pageOfs_ = 0;       // オフセットが分かるとき
            int pageOfsCount_ = -1; // オフセットからのページ数が分かるとき

            void dump(int page, int bankSize)
            {
                printf("%08x:p%02x:+%04x: bank%d, ",
                       page * bankSize + ofs_,
                       page, ofs_,
                       bank_);
                if (page_ >= 0)
                    printf("page %02x", page_);
                else
                {
                    printf("page i + %02x", pageOfs_);
                    if (pageOfsCount_ >= 0)
                        printf(" ~ %d", pageOfsCount_);
                }
                printf("\n");
            }
        };

        std::vector<Entry> entries_;
        int page_ = 0;
        int ofs_ = 0;

        void check(MapperType mapper, int ofs, const uint8_t *rom,
                   std::set<std::pair<int, int>> &fileOffsets)
        {
            int bankSize = 8192;
            int bankAddr[4] = {};
            int bankReg[4] = {};
            int bankCount = 4;

            switch (mapper)
            {
            default:
            case MAPPER_ASCII8KB:
                bankSize = 8 * 1024;
                bankCount = 4;
                bankAddr[0] = 0x4000;
                bankAddr[1] = 0x6000;
                bankAddr[2] = 0x8000;
                bankAddr[3] = 0xa000;
                bankReg[0] = 0x6000;
                bankReg[1] = 0x6800;
                bankReg[2] = 0x7000;
                bankReg[3] = 0x7800;
                break;

            case MAPPER_ASCII16KB:
                bankSize = 16 * 1024;
                bankCount = 2;
                bankAddr[0] = 0x4000;
                bankAddr[1] = 0x8000;
                bankReg[0] = 0x6000;
                bankReg[1] = 0x7000;
                break;

            case MAPPER_KONAMI8KB:
                bankSize = 8 * 1024;
                bankCount = 4;
                bankAddr[0] = 0x4000;
                bankAddr[1] = 0x6000;
                bankAddr[2] = 0x8000;
                bankAddr[3] = 0xa000;
                bankReg[0] = -1;
                bankReg[1] = 0x6000;
                bankReg[2] = 0x8000;
                bankReg[3] = 0xa000;
                break;

            case MAPPER_KONAMI8KB_SCC:
                bankSize = 8 * 1024;
                bankCount = 4;
                bankAddr[0] = 0x4000;
                bankAddr[1] = 0x6000;
                bankAddr[2] = 0x8000;
                bankAddr[3] = 0xa000;
                bankReg[0] = 0x5000;
                bankReg[1] = 0x7000;
                bankReg[2] = 0x9000;
                bankReg[3] = 0xb000;
                break;
            }

            page_ = ofs / bankSize;
            int pageOfs = page_ * bankSize;
            ofs -= pageOfs;
            ofs_ = ofs;
            rom += pageOfs;

            int regA = -1;
            int displaceA = 0;
            int maskA = -1;

            int jpCount = 0;
            int jpAddr = -1;

            while (ofs < bankSize && jpCount < 2)
            {
                auto c = rom[ofs];
                switch (c)
                {
                case 0xc6: // add #imm
                    if (bankSize - ofs < 2)
                        return;
                    {
                        int imm = rom[ofs + 1];
                        regA = add(regA, imm);
                        displaceA = add(displaceA, imm);
                        // mask は維持
                    }
                    ofs += 2;
                    break;

                case 0xe6: // and #imm
                    if (bankSize - ofs < 2)
                        return;
                    maskA = rom[ofs + 1];
                    if (regA > 0)
                        regA = regA & maskA;
                    displaceA = 0;
                    ofs += 2;
                    break;

                case 0x07: // rlca
                    regA = rotL(regA);
                    maskA = rotL(maskA);
                    displaceA = 0;
                    ++ofs;
                    break;

                case 0x3c: // inc a
                    regA = add(regA, 1);
                    displaceA = add(displaceA, 1);
                    // mask は維持
                    ++ofs;
                    break;

                case 0x3e: // ld	a, #imm
                    if (bankSize - ofs < 2)
                        return;
                    regA = rom[ofs + 1];
                    maskA = -1;
                    displaceA = 0;
                    ofs += 2;
                    break;

                case 0xaf: // xor	a
                    regA = 0;
                    maskA = -1;
                    displaceA = 0;
                    ++ofs;
                    break;

                case 0x32: // ld	(imm), a
                    if (bankSize - ofs < 2)
                        return;
                    {
                        int addr = rom[ofs + 1] | (rom[ofs + 2] << 8);
                        for (int i = 0; i < bankCount; ++i)
                        {
                            if (addr == bankReg[i])
                            {
                                if (mapper == MAPPER_KONAMI8KB_SCC &&
                                    i == 2 && regA == 63)
                                    break; // SCC enable

                                int fileOfs = page_ * bankSize + ofs;
                                if (fileOffsets.count({fileOfs, jpAddr}))
                                    return;

                                Entry e;
                                e.ofs_ = ofs;
                                e.bank_ = i;
                                e.page_ = regA;
                                e.pageOfs_ = displaceA;
                                e.pageOfsCount_ = maskToCount(maskA);
                                entries_.push_back(e);
                                e.dump(page_, bankSize);

                                fileOffsets.insert({fileOfs, jpAddr});
                                break;
                            }
                        }
                    }
                    ofs += 3;
                    break;

                case 0x18: // jr	d
                    if (bankSize - ofs < 1)
                        return;
                    {
                        int8_t d = rom[ofs + 1];
                        jpAddr = ofs;
                        ofs = ofs + 2 + d;
                        if (ofs < 0 || ofs >= bankSize)
                            return;
                        ++jpCount;
                    }
                    break;

                case 0xc3: // jp	d
                case 0xcd: // call	d
                    if (bankSize - ofs < 2)
                        return;
                    {
                        jpAddr = ofs;
                        ofs = (rom[ofs + 1] + (rom[ofs + 2] << 8)) - 0x4000; // ん？w
                        if (ofs < 0 || ofs >= bankSize)
                            return;
                        ++jpCount;
                    }
                    break;

                case 0x77: // ld (hl),a
                case 0x23: // inc hl
                case 0x2c: // inc l
                case 0xfb: // ei
                case 0xf3: // di
                case 0xe5: // push hl
                    ofs += 1;
                    break;

                case 0x21: // ld hl, #imm
                    ofs += 3;
                    break;

                default:
                    return;
                    //                    ++ofs;
                };
            }
        }

        void makePagePairSimple(std::set<PagePair> &pagePairs,
                                const BankInfo bankInfos_[4],
                                int nPages)
        {
            auto complete = [](PagePair pp[2], int b0, int b1, int b2, int b3) {
                if (pp[0].isValid())
                {
                    if (pp[0].pages_[0] < 0)
                        pp[0].pages_[0] = b0;
                    if (pp[0].pages_[1] < 0)
                        pp[0].pages_[1] = b1;
                }
                if (pp[1].isValid())
                {
                    if (pp[1].pages_[0] < 0)
                        pp[1].pages_[0] = b2;
                    if (pp[1].pages_[1] < 0)
                        pp[1].pages_[1] = b3;
                }
            };

            if (entries_.empty())
                return;

            auto &be = entries_.front();
            // 先頭と後続はおなじ特性と仮定

            if (be.page_ >= 0)
            {
                PagePair pp[2];
                for (auto &e : entries_)
                {
                    if (e.page_ >= 0)
                        pp[e.bank_ >> 1].pages_[e.bank_ & 1] = e.page_;
                    else
                        printf("unexpected pattern (page>0)\n");
                }

                complete(pp, 0, 1, 2, 3);

                for (auto &p : pp)
                    if (p.isValid())
                        pagePairs.insert(p);
            }
            else if (be.pageOfsCount_ > 0)
            {
                for (int i = 0; i < be.pageOfsCount_; ++i)
                {
                    PagePair pp[2];
                    for (auto &e : entries_)
                    {
                        pp[e.bank_ >> 1].pages_[e.bank_ & 1] = e.pageOfs_ + i;
                    }

                    complete(pp, 0, 1, 2, 3);

                    for (auto &p : pp)
                        if (p.isValid())
                            pagePairs.insert(p);
                }
            }
            else
            {
                for (int i = 0; i < nPages; ++i)
                {
                    PagePair pp[2];
                    for (auto &e : entries_)
                    {
                        int p = e.pageOfs_ + i;
                        if (bankInfos_[e.bank_].perhaps_.count(p))
                            pp[e.bank_ >> 1].pages_[e.bank_ & 1] = p;
                    }

                    complete(pp, 0, 1, 2, 3);

                    for (auto &p : pp)
                        if (p.isValid())
                            pagePairs.insert(p);
                }
            }
        }
    };

    std::vector<Record> records_;
    BankInfo bankInfos_[4];

    int bankSize_ = 0;
    int nPages_ = 0;

    std::set<PagePair> pagePairs_;

public:
    void addPagePair(int a, int b)
    {
        PagePair tmppp;
        tmppp.pages_[0] = a;
        tmppp.pages_[1] = b;
        pagePairs_.insert(tmppp);
    }

    static MapperType getMapperType(const char *str)
    {
        if (strcmp(str, "ascii8k") == 0)
            return MAPPER_ASCII8KB;
        else if (strcmp(str, "ascii16k") == 0)
            return MAPPER_ASCII16KB;
        else if (strcmp(str, "konami8k") == 0)
            return MAPPER_KONAMI8KB;
        else if (strcmp(str, "scc") == 0)
            return MAPPER_KONAMI8KB_SCC;
        else
        {
            printf("invalid mapper type '%s'\n", str);
            exit(-1);
        }
        return MAPPER_ASCII8KB;
    }

    void analyze(const std::vector<uint8_t> &rom)
    {
        std::set<std::pair<int, int>> fileOffsets; // { addr, jpOrigin }
        auto mapper = getMapperType(mapper_);
        printf("mapper type = %d\n", mapper);
        bankSize_ = getBankSize(mapper);
        nPages_ = ((int)rom.size() + bankSize_ - 1) / bankSize_;

        for (int ofs = 0; ofs < (int)rom.size(); ++ofs)
        {
            if ((ofs & 65535) == 0)
                printf("analyzing addr %08x...\n", ofs);

            Record r;
            r.check(mapper, ofs, rom.data(), fileOffsets);
            if (!r.entries_.empty())
            {
                records_.push_back(r);
                printf("---\n");
            }
        }

        printf("%zd records.\n", records_.size());

        std::set<int> usedPage;
        bankInfos_[0].certainly_.insert(0);
        usedPage.insert(0);

        for (auto &r : records_)
        {
            for (auto &e : r.entries_)
            {
                if (e.page_ >= 0)
                {
                    usedPage.insert(e.page_);
                    bankInfos_[e.bank_].certainly_.insert(e.page_);
                }
                else if (e.pageOfsCount_ >= 0)
                {
                    for (int i = 0; i < e.pageOfsCount_; ++i)
                    {
                        int p = i + e.pageOfs_;
                        usedPage.insert(p);
                        bankInfos_[e.bank_].certainly_.insert(p);
                    }
                }
            }
        }
        for (auto &r : records_)
        {
            for (auto &e : r.entries_)
            {
                if (e.page_ < 0 && e.pageOfsCount_ < 0)
                {
                    for (int i = e.pageOfs_; i < nPages_; ++i)
                    {
                        if (usedPage.count(i) == 0)
                        {
                            bankInfos_[e.bank_].perhaps_.insert(i);
                        }
                    }
                }
            }
        }

        //
        for (int i = 0; i < 4; ++i)
        {
            printf("Bank %d:\n", i);
            auto &bi = bankInfos_[i];
            printf("  cert : ");
            for (auto p : bi.certainly_)
                printf("%d ", p);
            printf("\n");
            printf("  perh : ");
            for (auto p : bi.perhaps_)
                printf("%d ", p);
            printf("\n");
        }
    }

    void dumpPagePairs()
    {
        for (auto &pp : pagePairs_)
            printf("(%d, %d)\n", pp.pages_[0], pp.pages_[1]);
        printf("%zd pages.\n", pagePairs_.size());
    }

    // シンプルな戦略で作成
    void makePagePairSimple()
    {
        addPagePair(0, 0);

        for (auto &r : records_)
        {
            r.makePagePairSimple(pagePairs_, bankInfos_, nPages_);
        }

        dumpPagePairs();
    }

    void makeROM(std::vector<uint8_t> &bios,
                 std::vector<uint8_t> &rom)
    {
        // 0x37fe	1	pageset(bank2) ofs
        // 0x37ff	1	pageset(bank2) count
        // 0x3800	64	bank1 table
        // 0x3900	128	bank2a table
        // 0x3a00	128	bank2b table
        // 0x3b00	128	pageset(bank2) table
        // pagesetは bank2b bank2a の順

        memset(&bios[0x3800], 0, 0x3c00 - 0x3800);

        for (auto &str : addPages_)
        {
            int p0, p1;
            sscanf(str.c_str(), "%02x%02x", &p0, &p1);
            printf("add page pair (%d, %d)\n", p0, p1);
            addPagePair(p0, p1);
        }

        int pageSet2Begin = INT_MAX;
        int nPageSet2 = 0;
        int i = 2;
        std::vector<uint8_t> processedMask(256, 0);
        for (auto &pp : pagePairs_)
        {
            if (pp.pages_[0] == 0)
                bios[0x3800 + pp.pages_[1]] = i;
            else
            {
                if (!(processedMask[pp.pages_[0]] & 1))
                {
                    processedMask[pp.pages_[0]] |= 1;
                    bios[0x3900 + pp.pages_[0] * 2 + 0] = i;
                    bios[0x3900 + pp.pages_[0] * 2 + 1] = pp.pages_[1];
                }

                if (!(processedMask[pp.pages_[1]] & 2))
                {
                    processedMask[pp.pages_[1]] |= 2;
                    bios[0x3a00 + pp.pages_[1] * 2 + 0] = i;
                    bios[0x3a00 + pp.pages_[1] * 2 + 1] = pp.pages_[0];
                }

                bios[0x3b00 + nPageSet2 * 2 + 0] = pp.pages_[1];
                bios[0x3b00 + nPageSet2 * 2 + 1] = pp.pages_[0];

                pageSet2Begin = std::min(i, pageSet2Begin);
                ++nPageSet2;
            }
            ++i;
        }

        bios[0x37fe] = pageSet2Begin;
        bios[0x37ff] = nPageSet2;

        printf("Bank1 table:\n");
        for (int i = 0; i < 64; ++i)
            printf("%02x ", bios[0x3800 + i]);
        printf("\n");

        printf("Bank1a table:\n");
        for (int i = 0; i < 64; ++i)
        {
            int a = bios[0x3900 + i * 2 + 0];
            int b = bios[0x3900 + i * 2 + 1];
            if (a == 0 && b == 0)
                printf("x ");
            else
                printf("%02x:%02x ", a, b);
        }
        printf("\n");

        printf("Bank1b table:\n");
        for (int i = 0; i < 64; ++i)
        {
            int a = bios[0x3a00 + i * 2 + 0];
            int b = bios[0x3a00 + i * 2 + 1];
            if (a == 0 && b == 0)
                printf("x ");
            else
                printf("%02x:%02x ", a, b);
        }
        printf("\n");

        printf("Pageset2 ofs: %d, pageset2 count: %d\n",
               pageSet2Begin, nPageSet2);
        for (int i = 0; i < nPageSet2; ++i)
            printf("%04x ",
                   bios[0x3b00 + i * 2 + 0] | (bios[0x3b00 + i * 2 + 1] << 8));
        printf("\n");

        ////////////////
        static constexpr int change_bank1 = 0x3c00;
        static constexpr int change_bank2 = 0x3c0c;
        static constexpr int change_bank3 = 0x3c34;
        static constexpr int cur_bank_addr = 0xf65c;

        printf("patch bank change\n");
        for (auto &r : records_)
        {
            int n = (int)r.entries_.size();
            for (int i = 0; i < n; ++i)
            {
                bool last = i == n - 1; // 連続したサブバンク切り替えでのページ探索を1回にしたい
                auto &e = r.entries_[i];
                auto ofs = r.page_ * bankSize_ + e.ofs_;

                switch (e.bank_)
                {
                case 1:
                    rom[ofs + 0] = 0xcd;
                    rom[ofs + 1] = change_bank1 & 255;
                    rom[ofs + 2] = (change_bank1 >> 8) & 255;
                    break;

                case 2:
                    if (last)
                    {
                        rom[ofs + 0] = 0xcd;
                        rom[ofs + 1] = change_bank2 & 255;
                        rom[ofs + 2] = (change_bank2 >> 8) & 255;
                    }
                    else
                    {
                        rom[ofs + 1] = (cur_bank_addr + 2) & 255;
                        rom[ofs + 2] = (cur_bank_addr >> 8) & 255;
                    }
                    break;

                case 3:
                    if (last)
                    {
                        rom[ofs + 0] = 0xcd;
                        rom[ofs + 1] = change_bank3 & 255;
                        rom[ofs + 2] = (change_bank3 >> 8) & 255;
                    }
                    else
                    {
                        rom[ofs + 1] = (cur_bank_addr + 3) & 255;
                        rom[ofs + 2] = (cur_bank_addr >> 8) & 255;
                    }
                    break;
                }
            }
        }

        ////////////////
        printf("make new ROM\n");
        std::vector<uint8_t> newRom(16384 * pagePairs_.size());
        i = 0;
        for (auto &pp : pagePairs_)
        {
            if (pp.pages_[0] >= 0)
                memcpy(&newRom[i * 16384],
                       &rom[pp.pages_[0] * bankSize_],
                       bankSize_);
            if (pp.pages_[0] >= 0)
                memcpy(&newRom[i * 16384 + bankSize_],
                       &rom[pp.pages_[1] * bankSize_],
                       bankSize_);
            ++i;
        }
        swap(newRom, rom);

        printf("new rom size = %zd\n", rom.size());
    }

    void makeROM_16KBMapper(std::vector<uint8_t> &rom)
    {
        // todo...
        static constexpr int change_bank0 = 0x3c4a;
        static constexpr int change_bank1 = 0x3c52;
        
        printf("patch bank change\n");
        for (auto &r : records_)
        {
            int n = (int)r.entries_.size();
            for (int i = 0; i < n; ++i)
            {
                auto &e = r.entries_[i];
                auto ofs = r.page_ * bankSize_ + e.ofs_;

                switch (e.bank_)
                {
                case 0:
                    rom[ofs + 0] = 0xcd;
                    rom[ofs + 1] = change_bank0 & 255;
                    rom[ofs + 2] = (change_bank0 >> 8) & 255;
                    break;

                case 1:
                    rom[ofs + 0] = 0xcd;
                    rom[ofs + 1] = change_bank1 & 255;
                    rom[ofs + 2] = (change_bank1 >> 8) & 255;
                    break;

                default:
                    printf("  invalid bank %d.\n", e.bank_);
                    break;
                }
            }
        }
    }


};

int getKeyAddr(const std::string &str)
{
    const char *keys[] =
        {
            "0",
            "1",
            "2",
            "3",
            "4",
            "5",
            "6",
            "7",
            "8",
            "9",
            "-",
            "^",
            "\\",
            "@",
            "[",
            ";",
            ":",
            "]",
            ",",
            ".",
            "/",
            "_",
            "a",
            "b",
            "c",
            "d",
            "e",
            "f",
            "g",
            "h",
            "i",
            "j",
            "k",
            "l",
            "m",
            "n",
            "o",
            "p",
            "q",
            "r",
            "s",
            "t",
            "u",
            "v",
            "w",
            "x",
            "y",
            "z",
            "shift",
            "ctrl",
            "graph",
            "caps",
            "kana",
            "f1",
            "f2",
            "f3",
            "f4",
            "f5",
            "esc",
            "tab",
            "stop",
            "bs",
            "select",
            "return",
            "space",
            "home",
            "ins",
            "del",
            "left",
            "up",
            "down",
            "right",
        };
    int i = 0;
    bool found = false;
    for (auto k : keys)
    {
        if (str == k)
        {
            found = true;
            break;
        }
        ++i;
    }
    if (!found)
    {
        printf("unknown key %s\n", str.c_str());
        return -1;
    }
    return i;
};

uint32_t paletteData_[] =
    {
        0,
        0x000000,
        0x22DD22,
        0x66FF66,
        0x2222FF,
        0x4466FF,
        0xAA2222,
        0x44DDFF,
        0xFF2222,
        0xFF6666,
        0xDDDD22,
        0xDDDD88,
        0x228822,
        0xDD44AA,
        0xAAAAAA,
        0xFFFFFF,
};

bool checkArg()
{
    if (!output_ ||
        !bios_ ||
        !biosPatch_)
        return false;
    return true;
}

int main(int argc, char **argv)
{
    if (!analyzeCommandLine(argc, argv) || !checkArg())
    {
        printUsage(argv[0]);
        return -1;
    }

    std::vector<uint8_t> bios;
    load(bios, bios_);

    std::vector<uint8_t> rom;
    if (rom_)
        load(rom, rom_);

    for (auto &str : romPatches_)
    {
        int addr, data;
        sscanf(str.c_str(), "%x:%x", &addr, &data);
        printf("rom patch %08x : %02x\n", addr, data);
        rom[addr] = data;
    }

    std::vector<uint8_t> patch;
    load(patch, biosPatch_);

    replaceArea(bios, patch, 0x0, 0x8);
    replaceArea(bios, patch, 0x66, 0x69);
    replaceArea(bios, patch, 0x38, 0x3b);
    replaceArea(bios, patch, 0x2d7, 0x39f);
    replaceArea(bios, patch, 0x19f1, 0x1aff); // wrtpsg_emu
    replaceArea(bios, patch, 0x1452, 0x145b); // snsmat
    if (reversePSGToneOrder_)
    {
        if (patch[0x1a7a] != 0x20)
        {
            printf("invalid psg emu.\n");
            exit(-1);
        }
        bios[0x1a7a] = 0x28;
    }
    replaceArea(bios, patch, 0x110e, 0x1113); // RDPSG
    replaceArea(bios, patch, 0x0093, 0x0096); // WRTPSG
    replaceArea(bios, patch, 0x1102, 0x1105); // WRTPSG
    replaceArea(bios, patch, 0x1b6, 0x1b9);   // RDSLT
    if (rom.size())
        replaceArea(bios, patch, 0x7d75, 0x7d78); // rom へJUMP. basic用ならなし
    replacePattern2(bios, 0xd3, 0x98, 0xd3, 0xbe);
    replacePattern2(bios, 0xdb, 0x98, 0xdb, 0xbe);
    replacePattern2(bios, 0xd3, 0x99, 0xd3, 0xbf);
    replacePattern2(bios, 0xdb, 0x99, 0xdb, 0xbf);

#if 0
    // 0x0004 が書き換えられてる疑惑
    bios[0x7ff0] = 0xbf;
    bios[0x7ff1] = 0x1b;
    replacePattern3(bios, 0x11, 0x04, 0x00,  0x11, 0xf0, 0x7f);
    replacePattern3(bios, 0x21, 0x04, 0x00,  0x21, 0xf0, 0x7f);
    replacePattern3(bios, 0x2a, 0x04, 0x00,  0x2a, 0xf0, 0x7f);
#endif

    // rst38h無効
    //    bios[0x38] = 0xc9;

    // palette
    static const int paletteAddr = 0x341;
    for (int i = 0; i < 32; ++i)
    {
        uint32_t c = paletteData_[i & 15];
        // BBGGRR
        int r = (c >> 16) & 255;
        int g = (c >> 8) & 255;
        int b = (c >> 0) & 255;
        int v = (r >> 6) | ((g >> 6) << 2) | ((b >> 6) << 4);
        bios[paletteAddr + i] = v;
    }

    {
        // ram アドレス
        uint8_t d[] = {0x21, 0x00, 0xe0, 0xc9};
        // ld hl, #e000h  ret
        writeBytes(bios, 0x7d5d, sizeof(d), d);
    }
    if (1)
    {
        uint8_t r0[] = {0xaf, 0xc9};       // A = 0
        uint8_t r1[] = {0xb7, 0xc9};       // CF = 0
        uint8_t r2[] = {0xf6, 0x01, 0xc9}; // ZF = 1
        uint8_t r3[] = {0xdd, 0xe9};       // JP (ix)
        writeBytes(bios, 0xc, sizeof(r0), r0);
        writeBytes(bios, 0x138, sizeof(r0), r0);
        bios[0x14] = 0xc9;
        bios[0x1c] = 0xc9;
        bios[0x24] = 0xc9;
        bios[0x30] = 0xc9;
        bios[0x13b] = 0xc9;

        writeBytes(bios, 0xe4, sizeof(r0), r0); // TAPIIN
        bios[0xe1] = 0xc9;                      // TAPION
        bios[0xea] = 0xc9;                      // TAPOON
        bios[0xed] = 0xc9;                      // TAPOUT

        writeBytes(bios, 0x46f, sizeof(r1), r1); // BREAKX
        // キー読み出し 0xd12 - 0xd69
        writeBytes(bios, 0xd12, sizeof(r2), r2);
        replaceArea(bios, patch, 0x1226, 0x122a); // read row 8

        // port選択
        bios[0x120c] = 0xc9;

        // CALBAS
        writeBytes(bios, 0x1ff, sizeof(r3), r3);
    }
    //    if (0)
    //    {
    //        // SNSMAT
    //        uint8_t r1[] = { 0x3e, 0xff, 0xc9 };
    //        writeBytes(bios, 0x141, sizeof(r1), r1);
    //    }

    for (int i = 0; i < 2 && i < (int)startKeys_.size(); ++i)
    {
        auto &str = startKeys_[i];
        auto r = getKeyAddr(str);
        int bit = r & 7;
        int row = r >> 3;
        printf("key %s = %d:%d\n", str.c_str(), bit, row);

        static constexpr int stkeyAddr = 0x383;
        static constexpr int keyBufAddr = 0xf580;

        auto addr = stkeyAddr + i * 5;
        bios[addr + 0] = 0x3e;
        bios[addr + 1] = ~(1 << bit);
        bios[addr + 2] = 0x32;
        bios[addr + 3] = (keyBufAddr & 255) + row;
        bios[addr + 4] = keyBufAddr >> 8;
    }

    if (mapper_)
    {
#if 0
        // ds4 item 99
        for(int i = 0; i < 16; ++i)
            rom[0x839b-0x4000 + i] = 99;
        // ds4 無敵
        rom[0x7278 - 0x4000] = 0xb7; // or a
        rom[0xb225 - 0x4000] = 0xb7; // or a (for boss)
#endif

        MEGAROMAnalyzer ma;
        ma.analyze(rom);
        
        if (ma.bankSize_ == 16384)
        {
            ma.makeROM_16KBMapper(rom);
        }
        else
        {
            ma.makePagePairSimple();
            
            // 37c8~ 2104byte位使っても大丈夫そう
            ma.makeROM(bios, rom);
        }
        replaceArea(bios, patch, 0x3c00, 0x4000);
    }

    bios.insert(bios.end(), rom.begin(), rom.end());
    swap(bios, rom);

    save(rom, output_);
    return 0;

    // 0x3c80 にブレーク入れてページを調べる
    // 0xf65c からbank
}

// dragon slayer 4: -t --pagepair 0e16 --pagepair 0e15

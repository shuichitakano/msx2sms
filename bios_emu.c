/* -*- mode:C; -*-
 *
 * bios_emu.c
 *
 * author(s) : Shuichi TAKANO
 * since 2016/09/12(Mon) 14:07:20
 */

#include <stdint.h>

void main()
{
}

uint8_t swapPortBit(uint8_t v)
{
    // SMS
    // 7: Joypad 2 Down
    // 6: Joypad 2 Up
    // 5: Joypad 1 Fire B
    // 4: Joypad 1 Fire A
    // 3: Joypad 1 Right
    // 2: Joypad 1 Left
    // 1: Joypad 1 Down
    // 0: Joypad 1 Up

    // MSX
    // 5: trigger b
    // 4: trigger a
    // 3: right
    // 2: left
    // 1: down
    // 0: up
    return v & 0x63;
}

uint8_t
RDPSG_emu(uint8_t port)
{
    if (port != 0xe)
        return 0;

    return swapPortBit(port); // まちがい
}

static uint8_t psgWork_[16];
static uint8_t curVol_[3];

void out(int r, int n)
{
}

void WRTPSG_emu(uint8_t reg, uint8_t data)
{
    // A	PSGのレジスタ番号
    // E	書き込むデータ

    uint8_t prevChStat = psgWork_[7];

    psgWork_[reg] = data;

    if (reg == 7)
    {
        // (0^1) ^ (0^1) = 1 ^ 1 = 0
        // (0^1) ^ (1^1) = 1 ^ 0 = 1
        // (1^1) ^ (0^1) = 0 ^ 1 = 1
        // (1^1) ^ (1^1) = 0 ^ 0 = 0

        uint8_t changed = prevChStat ^ data;
        uint8_t i;
        for (i = 0; i < 3; ++i)
        {
            // tone
            if (changed & 1)
            {
                if (data & 1)
                {
                    // key off
                    out(0x7f, (0x90 | 15) | (i << 5));
                }
                else
                {
                    // key on
                    uint8_t v = curVol_[i];
                    out(0x7f, 0x90 | (i << 5) | v);
                }
            }

            // noise
            if (changed & 8)
            {
                if (!(data & 8))
                {
                    // on
                    uint8_t v = curVol_[i];
                    out(0x7f, (0x90 | (3 << 5)) | v);
                }
            }

            data >>= 1;
            changed >>= 1;
        }

        if ((changed & 7) && ((data & 7) == 7))
        {
            // noise off
            out(0x7f, 0x90 | (3 << 5) | 15);
        }
    }
    else if (reg == 6)
    {
        // 16, 32, 64
        //   24, 48
        // 10000: 16
        // 01000: 8
        // 00100: 4

        data >>= 3;
        data &= 3;
        if (data == 3)
            data = 2;
        out(0x7f, 0xe4 | data);
    }
    else if (!(reg & 1) && reg < 6)
    {
        // 下のレジスタに書き込んだ時に反映
        uint8_t rbase = reg & 6;
        uint16_t v = ((psgWork_[rbase + 1] << 8) |
                      (psgWork_[rbase + 0]));
        while (v >= (1 << 10))
            v >>= 1;
        out(0x7f, 0x80 | (rbase << 4) | v & 15);
        out(0x7f, (v >> 4) & 63);
    }
    else
    {
        uint8_t regv = reg - 8;
        if (regv < 3)
        {
            uint8_t chstat = psgWork_[7] >> regv;
            uint8_t v = data ^ 15;
            if (v & 16)
                v = 0;
            curVol_[regv] = v;

            v |= 0x90 | (regv << 5);

            if (!(chstat & 1))
                out(0x7f, v);

            if (!(chstat & 8))
            {
                v |= (3 << 5);
                out(0x7f, v);
            }
        }
    }
}

///

// 0xf65c d e f

void changeBank1(uint8_t a)
{
    uint8_t p;

    *(uint8_t *)0xf65d = a;
    p = *(uint8_t *)(0x3800 + a);
    *(uint8_t *)0xfffe = p;
}

uint8_t
findPagePair(uint16_t hl)
{
    uint8_t ofs = *(uint8_t *)0x37fe;
    uint8_t n = *(uint8_t *)0x37ff;
    uint16_t tbl = 0x3b00;
    uint8_t left, right, mid;
    uint16_t mv;

    left = 0;
    right = n;
    while (left < right)
    {
        mid = (uint8_t)(left + right) >> (uint8_t)1;
        mv = *(uint16_t *)(tbl + (uint8_t)(mid << 1));
        if (mv == hl)
            return mid + ofs;
        else if (hl < mv)
            right = mid;
        else
            left = mid + 1;
    }
    return 0;
}

void changeBank2(uint8_t a)
{
    uint8_t p, crp3, rp3;
    uint16_t tbl;

    *(uint8_t *)0xf65e = a;
    crp3 = *(uint8_t *)0xf65f;

    tbl = 0x3900 + a * 2;
    p = *(uint8_t *)(tbl + 0);
    rp3 = *(uint8_t *)(tbl + 1);
    if (crp3 != rp3)
    {
        uint8_t pd = findPagePair(a | (crp3 << 8));
        if (pd != 0)
            p = pd;
    }
    *(uint8_t *)0xffff = p;
}

void changeBank3(uint8_t a)
{
    uint8_t p, crp2, rp2;
    uint16_t tbl;

    *(uint8_t *)0xf65f = a;
    crp2 = *(uint8_t *)0xf65e;

    tbl = 0x3a00 + a * 2;
    p = *(uint8_t *)(tbl + 0);
    rp2 = *(uint8_t *)(tbl + 1);
    if (crp2 != rp2)
    {
        uint8_t pd = findPagePair(crp2 | (a << 8));
        if (pd != 0)
            p = pd;
    }
    *(uint8_t *)0xffff = p;
}

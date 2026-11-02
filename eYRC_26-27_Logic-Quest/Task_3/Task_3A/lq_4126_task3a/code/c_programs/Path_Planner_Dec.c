/*
* Logic Quest (LQ): Task 3A Path Planner with Decryption
*
* Computes the shortest path on the Logic Quest arena (nodes N1..N36), with the
* line weights of Figure 1 on the Task 3A page: straight line 5, ring arcs
* 7 / 11 / 15 / 19 (inner ring to outermost ring), wavy line N1-N36 100.
*
* Inputs : current_node, next_node, is_encrypted
*   current_node     : where the bot is, 1..36. The path starts here.
*                      At the first location the bot is in the start box, N35.
*   is_encrypted = 0 : next_node is a node number.
*                      Path: current_node -> next_node.
*   is_encrypted = 1 : next_node is the 8-byte encrypted code, encrypted with
*                      PRESENT-80.
*                      Path: current_node -> the drop node in the block.
*
* Decrypted code (8 bytes, byte 0 first):
*   byte 0     : done flag (1 = last block: no pick-up after this drop)
*   byte 1..5  : message, 5 ASCII characters (0x00 = no character)
*   byte 6     : next pick-up node 1..36 (not used when the done flag is 1)
*   byte 7     : drop node 1..36 for this block
*   e.g. message "HELLO", drop at N26, then pick up at N12
*        -> 00 48 45 4C 4C 4F 0C 1A = 0048454C4C4F0C1A
*
* Outputs:
*   is_encrypted = 0 : the path from current_node to next_node.
*   is_encrypted = 1 : the message (message[]) and the path from current_node
*                      to the drop node. When the done flag is 0, the inputs
*                      for the next run are written too:
*                          IS_ENCRYPTED = 0
*                          CURRENT_NODE = the drop node
*                          NEXT_NODE    = the next pick-up node
*                      When the done flag is 1, the inputs are left as they are.
*
* Output (PC), read by test.sh line by line:
*   is_encrypted = 0     is_encrypted = 1, done = 0     is_encrypted = 1, done = 1
*   35                   Message : HELLO                Message : BYE
*   ...                  7                              7
*   7                    ...                            ...
*                        26                             26
*                        Next current_node : 26         Done
*                        Next next_node : 12
*   The path is one node per line, first node first. The message line shows
*   the non-zero message bytes as characters. The code at the end of main
*   prints all of this from message[], path_planned[], idx and the inputs
*   written for the next run.
*
* Debug prints: start every debug line with "dbg:" (see Functions Usage
*   below). test.sh and the evaluator skip the lines that start with "dbg:".
*   Any other extra line counts as part of your output and makes it wrong.
*
* PRESENT-80 (the cipher used for the encrypted codes):
*   block 64 bits, key 80 bits = A1 B2 C3 D4 E5 F6 07 18 29 3A (PRESENT_KEY_* below),
*   31 rounds. See the Task 3A page for the full description of the encryption;
*   your program has to do the reverse.
*
* Data memory ranges (used on the RISC-V CPU, Task 3B):
*   0x02000000 .. 0x0200000F : inputs
*   0x02000010 .. 0x0200001F : outputs (0x0200001C = CPU_DONE)
*   0x02000020 and up        : working memory, stack at the top
*
* Inputs  : ENCRYPTED_HI (0x02000000)  encrypted block, bytes 0..3 (first 8 hex digits)
*           ENCRYPTED_LO (0x02000004)  encrypted block, bytes 4..7 (last 8 hex digits)
*           IS_ENCRYPTED (0x02000008)  0 or 1                            } written for
*           NEXT_NODE    (0x0200000C)  next node, when IS_ENCRYPTED = 0   } the next run
*           CURRENT_NODE (0x0200000D)  current node (where the bot is)    } when done = 0
* Outputs : MESSAGE_0..4 (0x02000010 .. 0x02000014)  message, block bytes 1..5
*           NODE_POINT   (0x02000018)  every node of the path, written in order
*           CPU_DONE     (0x0200001C)  = 1 when finished
*
* Make sure you don't change anything outside the "Add your code here" sections.
*/

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>

// PRESENT-80 key: A1 B2 C3 D4 E5 F6 07 18 29 3A
#define PRESENT_KEY_HI  0xA1B2C3D4   // key bits 79..48
#define PRESENT_KEY_MID 0xE5F60718   // key bits 47..16
#define PRESENT_KEY_LO  0x293A       // key bits 15..0

#ifdef __linux__ // for host pc

    #include <stdio.h>

    void _put_byte(char c) { putchar(c); }

    void _put_str(char *str) {
        while (*str) {
            _put_byte(*str++);
        }
    }

    void print_output(uint8_t num) {
        if (num == 0) {
            putchar('0'); // if the number is 0, directly print '0'
            _put_byte('\n');
            return;
        }

        // convert the integer to a string
        char buffer[20];
        uint8_t index = 0;

        while (num > 0) {
            buffer[index++] = '0' + num % 10; // convert the last digit to its character representation
            num /= 10;                        // move to the next digit
        }
        // print the characters in reverse order (from right to left)
        while (index > 0) { putchar(buffer[--index]); }
        _put_byte('\n');
    }

    void _put_value(uint8_t val) { print_output(val); }

    // print a 32-bit value as 8 hex digits and a newline (e.g. the decrypted block)
    void _put_hex(uint32_t val) { printf("%08X\n", (unsigned int)val); }

    // read 16 hex digits into two 32-bit words, returns 0 if the text is not 16 hex digits
    int read_block(const char *hex, uint32_t *hi, uint32_t *lo) {
        uint32_t word[2] = { 0, 0 };
        if (hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X')) hex += 2;
        for (int n = 0; n < 16; n++) {
            char c = hex[n];
            uint32_t d;
            if      (c >= '0' && c <= '9') d = c - '0';
            else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
            else return 0;
            word[n >> 3] = (word[n >> 3] << 4) | d;
        }
        if (hex[16] != '\0') return 0;
        *hi = word[0];
        *lo = word[1];
        return 1;
    }

    /*
     * On the PC there is no data memory at 0x02000000, so MEM32(address) is a
     * normal array (one entry per 32-bit word). On the CPU it is the real
     * data memory. Either way, MEM32(0x02000020) reads the same place.
     */
    static uint32_t pc_data_mem[128];
    #define MEM32(addr)  (pc_data_mem[((addr) - 0x02000000u) >> 2])

#else  // for the test device

    void _put_byte(char c) { }
    void _put_value(uint8_t val) { }
    void _put_hex(uint32_t val) { }
    void _put_str(char *str) { }

    #define MEM32(addr)  (*(volatile uint32_t *)(addr))

#endif

/*
Functions Usage

To print while debugging, use the functions below, not printf().
On the PC they print. On the CPU they are empty, so they cost nothing there
and the same file still builds with generatehex.sh.

for a character: _put_byte('A');                        (putchar() on the PC)
for newline:     _put_byte('\n');
for string:      _put_str("your string here");
for number:      _put_value(your_number_here);           0 to 255, then a newline
for 32 bits:     _put_hex(your_32_bit_value_here);       8 hex digits, then a newline

Start every debug line with "dbg:" and end it with a newline (_put_value
and _put_hex end the line for you). test.sh and the evaluator skip the
lines that start with "dbg:", so these prints can stay in the file you
submit. Any other line you print counts as part of your output (see
"Output (PC)" at the top) and makes it wrong.

Examples:
        _put_str("dbg: start_point = ");     _put_value(start_point);
        _put_str("dbg: end_point = ");       _put_value(end_point);
        _put_str("dbg: ENCRYPTED_HI = ");    _put_hex(ENCRYPTED_HI);    // encrypted block, bytes 0..3
        _put_str("dbg: ENCRYPTED_LO = ");    _put_hex(ENCRYPTED_LO);    // encrypted block, bytes 4..7
        _put_str("dbg: first character = "); _put_byte(message[0]); _put_byte('\n');
        _put_str("dbg: Hello World!\n");

*/

// Custom Functions


// ############# Add your code here #############

// ---------- PRESENT-80 decryption (no arrays: all state in scalars) ----------
#define SBX(x, L, H) (((((x) < 8) ? (L) : (H)) >> (((x) & 7) << 2)) & 15)
#define SB(x)  SBX(x, 0xDA09B65Cu, 0x21748FE3u)   // S-box
#define ISB(x) SBX(x, 0xD21C8FE5u, 0xA970364Bu)   // inverse S-box

// decrypts hi:lo, writes message bytes 1..5 to msg[0..4],
// returns (done byte << 16) | (next pick-up << 8) | drop node
static uint32_t present_decrypt(uint32_t hi, uint32_t lo, uint8_t *msg) {
    // 80-bit key register as five 16-bit limbs, k0 = bits 79..64
    uint32_t k0 = PRESENT_KEY_HI >> 16,  k1 = PRESENT_KEY_HI & 0xFFFF;
    uint32_t k2 = PRESENT_KEY_MID >> 16, k3 = PRESENT_KEY_MID & 0xFFFF;
    uint32_t k4 = PRESENT_KEY_LO;
    uint32_t a, b, c, oh, ol, bit;
    uint8_t i, n, p;

    for (i = 1; i < 32; i++) {                       // forward key schedule -> K32
        a = k0; b = k1; c = k2;                      // rotate left 48
        k0 = k3; k1 = k4; k2 = a; k3 = b; k4 = c;
        a = k0;                                      // rotate left 13
        k0 = ((k0 << 13) | (k1 >> 3)) & 0xFFFF;
        k1 = ((k1 << 13) | (k2 >> 3)) & 0xFFFF;
        k2 = ((k2 << 13) | (k3 >> 3)) & 0xFFFF;
        k3 = ((k3 << 13) | (k4 >> 3)) & 0xFFFF;
        k4 = ((k4 << 13) | (a  >> 3)) & 0xFFFF;
        k0 = (k0 & 0x0FFF) | ((uint32_t)SB(k0 >> 12) << 12);
        k3 ^= i >> 1;
        k4 ^= (uint32_t)(i & 1) << 15;
    }
    hi ^= (k0 << 16) | k1;
    lo ^= (k2 << 16) | k3;
    for (i = 31; i > 0; i--) {
        oh = 0; ol = 0;                              // inverse permutation
        p = 0;
        for (n = 0; n < 63; n++) {
            bit = ((p >= 32) ? (hi >> (p - 32)) : (lo >> p)) & 1;
            if (n >= 32) oh |= bit << (n - 32); else ol |= bit << n;
            p += 16; if (p >= 63) p -= 63;
        }
        oh |= hi & 0x80000000u;                      // bit 63 stays
        hi = 0; lo = 0;
        for (n = 0; n < 32; n += 4) {                // inverse S-box layer
            hi |= (uint32_t)ISB((oh >> n) & 15) << n;
            lo |= (uint32_t)ISB((ol >> n) & 15) << n;
        }
        k3 ^= i >> 1;                                // undo key schedule step i
        k4 ^= (uint32_t)(i & 1) << 15;
        k0 = (k0 & 0x0FFF) | ((uint32_t)ISB(k0 >> 12) << 12);
        a = k4;                                      // rotate right 13
        k4 = ((k4 >> 13) | (k3 << 3)) & 0xFFFF;
        k3 = ((k3 >> 13) | (k2 << 3)) & 0xFFFF;
        k2 = ((k2 >> 13) | (k1 << 3)) & 0xFFFF;
        k1 = ((k1 >> 13) | (k0 << 3)) & 0xFFFF;
        k0 = ((k0 >> 13) | (a  << 3)) & 0xFFFF;
        a = k0; b = k1;                              // rotate right 48
        k0 = k2; k1 = k3; k2 = k4; k3 = a; k4 = b;
        hi ^= (k0 << 16) | k1;                       // add round key Ki
        lo ^= (k2 << 16) | k3;
    }
    msg[0] = hi >> 16; msg[1] = hi >> 8; msg[2] = hi;
    msg[3] = lo >> 24; msg[4] = lo >> 16;
    return ((hi >> 24) << 16) | (lo & 0xFFFF);
}

// ---------- shortest path: depth-first search over simple paths (no extra arrays) ----------
// The arena has only a few loops, so there are few simple paths. path_planned[] is the search stack.

// one undirected line a--b with weight w. r holds the best neighbour of u found so far
// (low byte = node, high byte = weight). Keeps the smallest neighbour of u that is > t.
static uint16_t cand(uint16_t r, uint8_t u, uint8_t t, uint8_t a, uint8_t b, uint8_t w) {
    uint8_t v = 0;
    if (u == a) v = b; else if (u == b) v = a;
    if (v > t && (r == 0 || v < (uint8_t)r)) r = v | ((uint16_t)w << 8);
    return r;
}

// smallest neighbour of u that is bigger than t: (weight << 8) | node, or 0 when there is none
static uint16_t adj(uint8_t u, uint8_t t) {
    uint16_t r = 0;
    #define E(a, b, w) r = cand(r, u, t, a, b, w);
    // wavy line
    E(36, 1, 100)
    // straight lines (5)
    E(1, 5, 5)    E(2, 4, 5)    E(3, 6, 5)    E(7, 10, 5)   E(8, 11, 5)
    E(9, 12, 5)   E(13, 14, 5)  E(14, 15, 5)  E(16, 17, 5)  E(18, 19, 5)
    E(20, 21, 5)  E(22, 25, 5)  E(23, 26, 5)  E(24, 27, 5)  E(29, 32, 5)
    E(28, 31, 5)  E(31, 34, 5)  E(30, 33, 5)
    // inner ring arcs (7)
    E(9, 11, 7)   E(11, 19, 7)  E(16, 23, 7)  E(23, 25, 7)
    // middle ring arcs (11)
    E(5, 7, 11)   E(5, 8, 11)   E(7, 15, 11)  E(8, 20, 11)
    E(20, 27, 11) E(27, 29, 11) E(29, 26, 11)
    // outer ring arcs (15)
    E(1, 4, 15)   E(1, 6, 15)   E(4, 14, 15)  E(32, 28, 15)
    // outermost ring arcs (19)
    E(13, 30, 19) E(30, 35, 19) E(35, 31, 19)
    #undef E
    return r;
}

// stop = 0 : returns the weight of the shortest path s -> e (0xFFFF if none)
// stop = 1 : best must be that weight; leaves a path of that weight in p[], returns its node count
static uint16_t dfs(uint8_t *p, uint8_t s, uint8_t e, uint16_t best, uint8_t stop) {
    uint16_t cost = 0, r;
    uint8_t d = 0, t = 0, v, j;
    p[0] = s;
    for (;;) {
        r = 0;
        if (p[d] != e) {
            r = adj(p[d], t);
            while (r) {                                  // skip neighbours already on the path, or too heavy
                v = r;
                for (j = 0; j <= d && p[j] != v; j++) { }
                if (j > d && cost + (r >> 8) <= best) break;
                r = adj(p[d], v);
            }
        } else {                                         // reached the end node
            if (cost < best) best = cost;
            if (stop) return d + 1;
        }
        if (r) {                                         // go one node deeper
            cost += r >> 8;
            p[++d] = r;
            t = 0;
        } else {                                         // dead end: go back one node
            if (d == 0) break;
            v = p[d];
            d--;
            cost -= adj(p[d], v - 1) >> 8;               // weight of the line we just left
            t = v;                                       // carry on with the next neighbour
        }
    }
    return stop ? 0 : best;
}

// ##############################################

// main function
int main(int argc, char const *argv[]) {

    #ifdef __linux__

        if (argc < 4) {
            _put_str("usage: ./Path_Planner_Dec <current_node> <next_node> <is_encrypted>\n");
            _put_str("       current_node     : where the bot is, e.g. 35 (the start box)\n");
            _put_str("       is_encrypted = 0 : next_node is a node number, e.g. 7\n");
            _put_str("       is_encrypted = 1 : next_node is the encrypted code, 16 hex digits\n");
            return 1;
        }
        uint8_t  CURRENT_NODE        = atoi(argv[1]);
        uint8_t  IS_ENCRYPTED        = atoi(argv[3]);
        uint8_t  NEXT_NODE           = 0;
        uint32_t ENCRYPTED_HI        = 0;
        uint32_t ENCRYPTED_LO        = 0;
        if (IS_ENCRYPTED) {
            if (!read_block(argv[2], &ENCRYPTED_HI, &ENCRYPTED_LO)) {
                _put_str("with is_encrypted = 1, next_node must be exactly 16 hex digits\n");
                return 1;
            }
        }
        else {
            NEXT_NODE = atoi(argv[2]);
        }
        uint8_t MESSAGE_0           = 0;
        uint8_t MESSAGE_1           = 0;
        uint8_t MESSAGE_2           = 0;
        uint8_t MESSAGE_3           = 0;
        uint8_t MESSAGE_4           = 0;
        uint8_t NODE_POINT          = 0;
        uint8_t CPU_DONE            = 0;

    #else
        // Address value of variables are updated for RISC-V Implementation
        // inputs: 0x02000000 .. 0x0200000F
        #define ENCRYPTED_HI        (* (volatile uint32_t * ) 0x02000000)
        #define ENCRYPTED_LO        (* (volatile uint32_t * ) 0x02000004)
        #define IS_ENCRYPTED        (* (volatile uint8_t  * ) 0x02000008)
        #define NEXT_NODE           (* (volatile uint8_t  * ) 0x0200000c)
        #define CURRENT_NODE        (* (volatile uint8_t  * ) 0x0200000d)
        // outputs: 0x02000010 .. 0x0200001F
        #define MESSAGE_0           (* (volatile uint8_t  * ) 0x02000010)
        #define MESSAGE_1           (* (volatile uint8_t  * ) 0x02000011)
        #define MESSAGE_2           (* (volatile uint8_t  * ) 0x02000012)
        #define MESSAGE_3           (* (volatile uint8_t  * ) 0x02000013)
        #define MESSAGE_4           (* (volatile uint8_t  * ) 0x02000014)
        #define NODE_POINT          (* (volatile uint8_t  * ) 0x02000018)
        #define CPU_DONE            (* (volatile uint8_t  * ) 0x0200001c)

    #endif

    // read once here: for the next run, the program may write IS_ENCRYPTED = 0
    uint8_t is_encrypted = IS_ENCRYPTED;

    // the message: bytes 1..5 of the decrypted block (0x00 = no character)
    uint8_t message[5] = { 0, 0, 0, 0, 0 };

    // array to store the planned path
    uint8_t path_planned[40];
    // index to keep track of the path_planned array
    uint8_t idx = 0;

    uint8_t start_point = CURRENT_NODE;   // where the bot is
    uint8_t end_point   = 0;              // where the bot goes

    // ############# Add your code here #############

    // set path_planned[] and idx, message[] when is_encrypted = 1, and the
    // inputs for the next run (see Outputs at the top of this file)

    uint8_t go_on = 0;                               // 1 when the next move needs inputs written
    uint8_t pick  = 0;                               // next pick-up node
    uint16_t best;
    if (is_encrypted) {
        uint32_t r = present_decrypt(ENCRYPTED_HI, ENCRYPTED_LO, message);
        end_point = r;                               // drop node
        pick      = r >> 8;
        go_on     = ((r >> 16) & 255) == 0;          // done flag 0
    } else {
        end_point = NEXT_NODE;
    }

    if (start_point >= 1 && start_point <= 36 && end_point >= 1 && end_point <= 36) {
        best = dfs(path_planned, start_point, end_point, 0xFFFF, 0);        // shortest weight
        if (best != 0xFFFF) idx = dfs(path_planned, start_point, end_point, best, 1);   // a path of that weight
    }

    if (go_on) {                                     // inputs for the next move
        IS_ENCRYPTED = 0;
        CURRENT_NODE = end_point;
        NEXT_NODE    = pick;
    }

    // ##############################################

    // the message bytes are written into data memory (encrypted block only).
    if (is_encrypted) {
        MESSAGE_0 = message[0];
        MESSAGE_1 = message[1];
        MESSAGE_2 = message[2];
        MESSAGE_3 = message[3];
        MESSAGE_4 = message[4];
    }
    // the node values are written into data memory sequentially.
    for (int i = 0; i < idx; ++i) {
        NODE_POINT = path_planned[i];
    }
    // Path Planning Computation Done Flag
    CPU_DONE = 1;

    #ifdef __linux__    // for host pc: the output test.sh reads (see "Output (PC)" at the top)

        if (is_encrypted) {
            _put_str("Message : ");
            for (int i = 0; i < 5; ++i) {
                if (message[i]) _put_byte(message[i]);
            }
            _put_byte('\n');
        }
        for (int i = 0; i < idx; ++i) {
            _put_value(path_planned[i]);
        }
        if (is_encrypted) {
            if (IS_ENCRYPTED == 0) {            // the inputs written for the next run
                _put_str("Next current_node : ");
                _put_value(CURRENT_NODE);
                _put_str("Next next_node : ");
                _put_value(NEXT_NODE);
            }
            else {
                _put_str("Done\n");
            }
        }

    #endif

    return 0;
}
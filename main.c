#include "defs.h"

// piece bitboards
U64 bitboards[12];

// occupancy bitboards
U64 occupancies[3];

// side to move
int side;

// enpassant square
int enpassant = no_sq; 

// castling rights
int castle;

// "almost" unique position identifier aka hash key or position key
U64 hash_key;

// positions repetition table
U64 repetition_table[1000];  // 1000 is a number of plies (500 moves) in the entire game

// repetition index
int repetition_index;

// half move counter
int ply;

int LMRTable[64][64];


/**********************************\
 ==================================
 
       Time controls variables
 
 ==================================
\**********************************/

// exit from engine flag
int quit = 0;

// UCI "movestogo" command moves counter
int movestogo = 30;

// UCI "movetime" command time counter
int movetime = -1;

// UCI "time" command holder (ms)
int uci_time = -1;

// UCI "inc" command's time increment holder
int inc = 0;

// UCI "starttime" command time holder
int starttime = 0;

// UCI "stoptime" command time holder
int stoptime = 0;

// variable to flag time control availability
int timeset = 0;

// variable to flag when the time is up
int stopped = 0;


/**********************************\
 ==================================
 
       Miscellaneous functions
          forked from VICE
         by Richard Allbert
 
 ==================================
\**********************************/

// get time in milliseconds
int get_time_ms()
{
        struct timeval time_value;
        gettimeofday(&time_value, NULL);
        return time_value.tv_sec * 1000 + time_value.tv_usec / 1000;
}

/*

  Function to "listen" to GUI's input during search.
  It's waiting for the user input from STDIN.
  OS dependent.
  
  First Richard Allbert aka BluefeverSoftware grabbed it from somewhere...
  And then Code Monkey King has grabbed it from VICE)
  
*/
  
int input_waiting()
{
    #ifndef WIN32
        fd_set readfds;
        struct timeval tv;
        FD_ZERO (&readfds);
        FD_SET (fileno(stdin), &readfds);
        tv.tv_sec=0; tv.tv_usec=0;
        select(16, &readfds, 0, 0, &tv);

        return (FD_ISSET(fileno(stdin), &readfds));
    #else
        static int init = 0, pipe;
        static HANDLE inh;
        DWORD dw;

        if (!init)
        {
            init = 1;
            inh = GetStdHandle(STD_INPUT_HANDLE);
            pipe = !GetConsoleMode(inh, &dw);
            if (!pipe)
            {
                SetConsoleMode(inh, dw & ~(ENABLE_MOUSE_INPUT|ENABLE_WINDOW_INPUT));
                FlushConsoleInputBuffer(inh);
            }
        }
        
        if (pipe)
        {
           if (!PeekNamedPipe(inh, NULL, 0, NULL, &dw, NULL)) return 1;
           return dw;
        }
        
        else
        {
           GetNumberOfConsoleInputEvents(inh, &dw);
           return dw <= 1 ? 0 : dw;
        }

    #endif
}

// read GUI/user input
void read_input()
{
    // bytes to read holder
    int bytes;
    
    // GUI/user input
    char input[256] = "", *endc;

    // "listen" to STDIN
    if (input_waiting())
    {
        // tell engine to stop calculating
        stopped = 1;
        
        // loop to read bytes from STDIN
        do
        {
            // read bytes from STDIN
            bytes=read(fileno(stdin), input, 256);
        }
        
        // until bytes available
        while (bytes < 0);
        
        // searches for the first occurrence of '\n'
        endc = strchr(input,'\n');
        
        // if found new line set value at pointer to 0
        if (endc) *endc=0;
        
        // if input is available
        if (strlen(input) > 0)
        {
            // match UCI "quit" command
            if (!strncmp(input, "quit", 4))
            {
                // tell engine to terminate exacution    
                quit = 1;
            }

            // // match UCI "stop" command
            else if (!strncmp(input, "stop", 4))    {
                // tell engine to terminate exacution
                quit = 1;
            }
        }   
    }
}

// a bridge function to interact between search and GUI input
static void communicate() {
	// if time is up break here
    if(timeset == 1 && get_time_ms() > stoptime) {
		// tell engine to stop calculating
		stopped = 1;
	}
	
    // read GUI input
	read_input();
}


/**********************************\
 ==================================
 
           Random numbers
 
 ==================================
\**********************************/


/**********************************\
 ==================================
 
          Bit manipulations
 
 ==================================
\**********************************/



/**********************************\
 ==================================
 
            Zobrist keys
 
 ==================================
\**********************************/

// random piece keys [piece][square]
U64 piece_keys[12][64];

// random enpassant keys [square]
U64 enpassant_keys[64];

// random castling keys
U64 castle_keys[16];

// random side key
U64 side_key;

// init random hash keys
void init_random_keys()
{
    // update pseudo random number state
    random_state = 1804289383;

    // loop over piece codes
    for (int piece = P; piece <= k; piece++)
    {
        // loop over board squares
        for (int square = 0; square < 64; square++)
            // init random piece keys
            piece_keys[piece][square] = get_random_U64_number();
    }
    
    // loop over board squares
    for (int square = 0; square < 64; square++)
        // init random enpassant keys
        enpassant_keys[square] = get_random_U64_number();
    
    // loop over castling keys
    for (int index = 0; index < 16; index++)
        // init castling keys
        castle_keys[index] = get_random_U64_number();
        
    // init random side key
    side_key = get_random_U64_number();
}

// generate "almost" unique position ID aka hash key from scratch
U64 generate_hash_key()
{
    // final hash key
    U64 final_key = 0ULL;
    
    // temp piece bitboard copy
    U64 bitboard;
    
    // loop over piece bitboards
    for (int piece = P; piece <= k; piece++)
    {
        // init piece bitboard copy
        bitboard = bitboards[piece];
        
        // loop over the pieces within a bitboard
        while (bitboard)
        {
            // init square occupied by the piece
            int square = get_ls1b_index(bitboard);
            
            // hash piece
            final_key ^= piece_keys[piece][square];
            
            // pop LS1B
            pop_bit(bitboard, square);
        }
    }
    
    // if enpassant square is on board
    if (enpassant != no_sq)
        // hash enpassant
        final_key ^= enpassant_keys[enpassant];
    
    // hash castling rights
    final_key ^= castle_keys[castle];
    
    // hash the side only if black is to move
    if (side == black) final_key ^= side_key;
    
    // return generated hash key
    return final_key;
}


/**********************************\
 ==================================
 
           Input & Output
 
 ==================================
\**********************************/

// print bitboard
void print_bitboard(U64 bitboard)
{
    // print offset
    printf("\n");

    // loop over board ranks
    for (int rank = 0; rank < 8; rank++)
    {
        // loop over board files
        for (int file = 0; file < 8; file++)
        {
            // convert file & rank into square index
            int square = rank * 8 + file;
            
            // print ranks
            if (!file)
                printf("  %d ", 8 - rank);
            
            // print bit state (either 1 or 0)
            printf(" %d", get_bit(bitboard, square) ? 1 : 0);
            
        }
        
        // print new line every rank
        printf("\n");
    }
    
    // print board files
    printf("\n     a b c d e f g h\n\n");
    
    // print bitboard as unsigned decimal number
    printf("     Bitboard: %llud\n\n", bitboard);
}

// print board
void print_board()
{
    // print offset
    printf("\n");

    // loop over board ranks
    for (int rank = 0; rank < 8; rank++)
    {
        // loop ober board files
        for (int file = 0; file < 8; file++)
        {
            // init square
            int square = rank * 8 + file;
            
            // print ranks
            if (!file)
                printf("  %d ", 8 - rank);
            
            // define piece variable
            int piece = -1;
            
            // loop over all piece bitboards
            for (int bb_piece = P; bb_piece <= k; bb_piece++)
            {
                // if there is a piece on current square
                if (get_bit(bitboards[bb_piece], square))
                    // get piece code
                    piece = bb_piece;
            }
            
            // print different piece set depending on OS
            #ifdef WIN64
                printf(" %c", (piece == -1) ? '.' : ascii_pieces[piece]);
            #else
                printf(" %s", (piece == -1) ? "." : unicode_pieces[piece]);
            #endif
        }
        
        // print new line every rank
        printf("\n");
    }
    
    // print board files
    printf("\n     a b c d e f g h\n\n");
    
    // print side to move
    printf("     Side:     %s\n", !side ? "white" : "black");
    
    // print enpassant square
    printf("     Enpassant:   %s\n", (enpassant != no_sq) ? square_to_coordinates[enpassant] : "no");
    
    // print castling rights
    printf("     Castling:  %c%c%c%c\n\n", (castle & wk) ? 'K' : '-',
                                           (castle & wq) ? 'Q' : '-',
                                           (castle & bk) ? 'k' : '-',
                                           (castle & bq) ? 'q' : '-');
    
    // print hash key
    printf("     Hash key:  %llx\n\n", hash_key);
}

// parse FEN string
void parse_fen(char *fen)
{
    // reset board position (bitboards)
    memset(bitboards, 0ULL, sizeof(bitboards));
    
    // reset occupancies (bitboards)
    memset(occupancies, 0ULL, sizeof(occupancies));
    
    // reset game state variables
    side = 0;
    enpassant = no_sq;
    castle = 0;
    
    // reset repetition index
    repetition_index = 0;
    
    // reset repetition table
    memset(repetition_table, 0ULL, sizeof(repetition_table));

    // loop over board ranks
    for (int rank = 0; rank < 8; rank++)
    {
        // loop over board files
        for (int file = 0; file < 8; file++)
        {
            // init current square
            int square = rank * 8 + file;
            
            // match ascii pieces within FEN string
            if ((*fen >= 'a' && *fen <= 'z') || (*fen >= 'A' && *fen <= 'Z'))
            {
                // init piece type
                int piece = char_pieces[*fen];
                
                // set piece on corresponding bitboard
                set_bit(bitboards[piece], square);
                
                // increment pointer to FEN string
                fen++;
            }
            
            // match empty square numbers within FEN string
            if (*fen >= '0' && *fen <= '9')
            {
                // init offset (convert char 0 to int 0)
                int offset = *fen - '0';
                
                // define piece variable
                int piece = -1;
                
                // loop over all piece bitboards
                for (int bb_piece = P; bb_piece <= k; bb_piece++)
                {
                    // if there is a piece on current square
                    if (get_bit(bitboards[bb_piece], square))
                        // get piece code
                        piece = bb_piece;
                }
                
                // on empty current square
                if (piece == -1)
                    // decrement file
                    file--;
                
                // adjust file counter
                file += offset;
                
                // increment pointer to FEN string
                fen++;
            }
            
            // match rank separator
            if (*fen == '/')
                // increment pointer to FEN string
                fen++;
        }
    }
    
    // got to parsing side to move (increment pointer to FEN string)
    fen++;
    
    // parse side to move
    (*fen == 'w') ? (side = white) : (side = black);
    
    // go to parsing castling rights
    fen += 2;
    
    // parse castling rights
    while (*fen != ' ')
    {
        switch (*fen)
        {
            case 'K': castle |= wk; break;
            case 'Q': castle |= wq; break;
            case 'k': castle |= bk; break;
            case 'q': castle |= bq; break;
            case '-': break;
        }

        // increment pointer to FEN string
        fen++;
    }
    
    // got to parsing enpassant square (increment pointer to FEN string)
    fen++;
    
    // parse enpassant square
    if (*fen != '-')
    {
        // parse enpassant file & rank
        int file = fen[0] - 'a';
        int rank = 8 - (fen[1] - '0');
        
        // init enpassant square
        enpassant = rank * 8 + file;
    }
    
    // no enpassant square
    else
        enpassant = no_sq;
    
    // loop over white pieces bitboards
    for (int piece = P; piece <= K; piece++)
        // populate white occupancy bitboard
        occupancies[white] |= bitboards[piece];
    
    // loop over black pieces bitboards
    for (int piece = p; piece <= k; piece++)
        // populate white occupancy bitboard
        occupancies[black] |= bitboards[piece];
    
    // init all occupancies
    occupancies[both] |= occupancies[white];
    occupancies[both] |= occupancies[black];
    
    // init hash key
    hash_key = generate_hash_key();
}


/**********************************\
 ==================================
 
              Attacks
 
 ==================================
\**********************************/

/* 
     not A file          not H file         not HG files      not AB files
      bitboard            bitboard            bitboard          bitboard

 8  0 1 1 1 1 1 1 1    1 1 1 1 1 1 1 0    1 1 1 1 1 1 0 0    0 0 1 1 1 1 1 1
 7  0 1 1 1 1 1 1 1    1 1 1 1 1 1 1 0    1 1 1 1 1 1 0 0    0 0 1 1 1 1 1 1
 6  0 1 1 1 1 1 1 1    1 1 1 1 1 1 1 0    1 1 1 1 1 1 0 0    0 0 1 1 1 1 1 1
 5  0 1 1 1 1 1 1 1    1 1 1 1 1 1 1 0    1 1 1 1 1 1 0 0    0 0 1 1 1 1 1 1
 4  0 1 1 1 1 1 1 1    1 1 1 1 1 1 1 0    1 1 1 1 1 1 0 0    0 0 1 1 1 1 1 1
 3  0 1 1 1 1 1 1 1    1 1 1 1 1 1 1 0    1 1 1 1 1 1 0 0    0 0 1 1 1 1 1 1
 2  0 1 1 1 1 1 1 1    1 1 1 1 1 1 1 0    1 1 1 1 1 1 0 0    0 0 1 1 1 1 1 1
 1  0 1 1 1 1 1 1 1    1 1 1 1 1 1 1 0    1 1 1 1 1 1 0 0    0 0 1 1 1 1 1 1
    
    a b c d e f g h    a b c d e f g h    a b c d e f g h    a b c d e f g h

*/



// pawn attacks table [side][square]
U64 pawn_attacks[2][64];

// knight attacks table [square]
U64 knight_attacks[64];

// king attacks table [square]
U64 king_attacks[64];

// bishop attack masks
U64 bishop_masks[64];

// rook attack masks
U64 rook_masks[64];

// bishop attacks table [square][occupancies]
U64 bishop_attacks[64][512];

// rook attacks rable [square][occupancies]
U64 rook_attacks[64][4096];


// init leaper pieces attacks
void init_leapers_attacks()
{
    // loop over 64 board squares
    for (int square = 0; square < 64; square++)
    {
        // init pawn attacks
        pawn_attacks[white][square] = mask_pawn_attacks(white, square);
        pawn_attacks[black][square] = mask_pawn_attacks(black, square);
        
        // init knight attacks
        knight_attacks[square] = mask_knight_attacks(square);
        
        // init king attacks
        king_attacks[square] = mask_king_attacks(square);
    }
}

// set occupancies
U64 set_occupancy(int index, int bits_in_mask, U64 attack_mask)
{
    // occupancy map
    U64 occupancy = 0ULL;
    
    // loop over the range of bits within attack mask
    for (int count = 0; count < bits_in_mask; count++)
    {
        // get LS1B index of attacks mask
        int square = get_ls1b_index(attack_mask);
        
        // pop LS1B in attack map
        pop_bit(attack_mask, square);
        
        // make sure occupancy is on board
        if (index & (1 << count))
            // populate occupancy map
            occupancy |= (1ULL << square);
    }
    
    // return occupancy map
    return occupancy;
}


/**********************************\
 ==================================
 
               Magics
 
 ==================================
\**********************************/

// find appropriate magic number
U64 find_magic_number(int square, int relevant_bits, int bishop)
{
    // init occupancies
    U64 occupancies[4096];
    
    // init attack tables
    U64 attacks[4096];
    
    // init used attacks
    U64 used_attacks[4096];
    
    // init attack mask for a current piece
    U64 attack_mask = bishop ? mask_bishop_attacks(square) : mask_rook_attacks(square);
    
    // init occupancy indicies
    int occupancy_indicies = 1 << relevant_bits;
    
    // loop over occupancy indicies
    for (int index = 0; index < occupancy_indicies; index++)
    {
        // init occupancies
        occupancies[index] = set_occupancy(index, relevant_bits, attack_mask);
        
        // init attacks
        attacks[index] = bishop ? bishop_attacks_on_the_fly(square, occupancies[index]) :
                                    rook_attacks_on_the_fly(square, occupancies[index]);
    }
    
    // test magic numbers loop
    for (int random_count = 0; random_count < 100000000; random_count++)
    {
        // generate magic number candidate
        U64 magic_number = generate_magic_number();
        
        // skip inappropriate magic numbers
        if (count_bits((attack_mask * magic_number) & 0xFF00000000000000) < 6) continue;
        
        // init used attacks
        memset(used_attacks, 0ULL, sizeof(used_attacks));
        
        // init index & fail flag
        int index, fail;
        
        // test magic index loop
        for (index = 0, fail = 0; !fail && index < occupancy_indicies; index++)
        {
            // init magic index
            int magic_index = (int)((occupancies[index] * magic_number) >> (64 - relevant_bits));
            
            // if magic index works
            if (used_attacks[magic_index] == 0ULL)
                // init used attacks
                used_attacks[magic_index] = attacks[index];
            
            // otherwise
            else if (used_attacks[magic_index] != attacks[index])
                // magic index doesn't work
                fail = 1;
        }
        
        // if magic number works
        if (!fail)
            // return it
            return magic_number;
    }
    
    // if magic number doesn't work
    printf("  Magic number fails!\n");
    return 0ULL;
}

// init magic numbers
void init_magic_numbers()
{
    // loop over 64 board squares
    for (int square = 0; square < 64; square++)
        // init rook magic numbers
        rook_magic_numbers[square] = find_magic_number(square, rook_relevant_bits[square], rook);

    // loop over 64 board squares
    for (int square = 0; square < 64; square++)
        // init bishop magic numbers
        bishop_magic_numbers[square] = find_magic_number(square, bishop_relevant_bits[square], bishop);
}

// init slider piece's attack tables
void init_sliders_attacks(int bishop)
{
    // loop over 64 board squares
    for (int square = 0; square < 64; square++)
    {
        // init bishop & rook masks
        bishop_masks[square] = mask_bishop_attacks(square);
        rook_masks[square] = mask_rook_attacks(square);
        
        // init current mask
        U64 attack_mask = bishop ? bishop_masks[square] : rook_masks[square];
        
        // init relevant occupancy bit count
        int relevant_bits_count = count_bits(attack_mask);
        
        // init occupancy indicies
        int occupancy_indicies = (1 << relevant_bits_count);
        
        // loop over occupancy indicies
        for (int index = 0; index < occupancy_indicies; index++)
        {
            // bishop
            if (bishop)
            {
                // init current occupancy variation
                U64 occupancy = set_occupancy(index, relevant_bits_count, attack_mask);
                
                // init magic index
                int magic_index = (occupancy * bishop_magic_numbers[square]) >> (64 - bishop_relevant_bits[square]);
                
                // init bishop attacks
                bishop_attacks[square][magic_index] = bishop_attacks_on_the_fly(square, occupancy);
            }
            
            // rook
            else
            {
                // init current occupancy variation
                U64 occupancy = set_occupancy(index, relevant_bits_count, attack_mask);
                
                // init magic index
                int magic_index = (occupancy * rook_magic_numbers[square]) >> (64 - rook_relevant_bits[square]);
                
                // init rook attacks
                rook_attacks[square][magic_index] = rook_attacks_on_the_fly(square, occupancy);
            
            }
        }
    }
}

// get bishop attacks
static inline U64 get_bishop_attacks(int square, U64 occupancy)
{
    // get bishop attacks assuming current board occupancy
    occupancy &= bishop_masks[square];
    occupancy *= bishop_magic_numbers[square];
    occupancy >>= 64 - bishop_relevant_bits[square];
    
    // return bishop attacks
    return bishop_attacks[square][occupancy];
}

// get rook attacks
static inline U64 get_rook_attacks(int square, U64 occupancy)
{
    // get rook attacks assuming current board occupancy
    occupancy &= rook_masks[square];
    occupancy *= rook_magic_numbers[square];
    occupancy >>= 64 - rook_relevant_bits[square];
    
    // return rook attacks
    return rook_attacks[square][occupancy];
}

// get queen attacks
static inline U64 get_queen_attacks(int square, U64 occupancy)
{
    // init result attacks bitboard
    U64 queen_attacks = 0ULL;
    
    // init bishop occupancies
    U64 bishop_occupancy = occupancy;
    
    // init rook occupancies
    U64 rook_occupancy = occupancy;
    
    // get bishop attacks assuming current board occupancy
    bishop_occupancy &= bishop_masks[square];
    bishop_occupancy *= bishop_magic_numbers[square];
    bishop_occupancy >>= 64 - bishop_relevant_bits[square];
    
    // get bishop attacks
    queen_attacks = bishop_attacks[square][bishop_occupancy];
    
    // get rook attacks assuming current board occupancy
    rook_occupancy &= rook_masks[square];
    rook_occupancy *= rook_magic_numbers[square];
    rook_occupancy >>= 64 - rook_relevant_bits[square];
    
    // get rook attacks
    queen_attacks |= rook_attacks[square][rook_occupancy];
    
    // return queen attacks
    return queen_attacks;
}


/**********************************\
 ==================================
 
           Move generator
 
 ==================================
\**********************************/

// is square current given attacked by the current given side
static inline int is_square_attacked(int square, int side)
{
    // attacked by white pawns
    if ((side == white) && (pawn_attacks[black][square] & bitboards[P])) return 1;
    
    // attacked by black pawns
    if ((side == black) && (pawn_attacks[white][square] & bitboards[p])) return 1;
    
    // attacked by knights
    if (knight_attacks[square] & ((side == white) ? bitboards[N] : bitboards[n])) return 1;
    
    // attacked by bishops
    if (get_bishop_attacks(square, occupancies[both]) & ((side == white) ? bitboards[B] : bitboards[b])) return 1;

    // attacked by rooks
    if (get_rook_attacks(square, occupancies[both]) & ((side == white) ? bitboards[R] : bitboards[r])) return 1;    

    // attacked by bishops
    if (get_queen_attacks(square, occupancies[both]) & ((side == white) ? bitboards[Q] : bitboards[q])) return 1;
    
    // attacked by kings
    if (king_attacks[square] & ((side == white) ? bitboards[K] : bitboards[k])) return 1;

    // by default return false
    return 0;
}

// print attacked squares
void print_attacked_squares(int side)
{
    printf("\n");
    
    // loop over board ranks
    for (int rank = 0; rank < 8; rank++)
    {
        // loop over board files
        for (int file = 0; file < 8; file++)
        {
            // init square
            int square = rank * 8 + file;
            
            // print ranks
            if (!file)
                printf("  %d ", 8 - rank);
            
            // check whether current square is attacked or not
            printf(" %d", is_square_attacked(square, side) ? 1 : 0);
        }
        
        // print new line every rank
        printf("\n");
    }
    
    // print files
    printf("\n     a b c d e f g h\n\n");
}

/*
          binary move bits                               hexidecimal constants
    
    0000 0000 0000 0000 0011 1111    source square       0x3f
    0000 0000 0000 1111 1100 0000    target square       0xfc0
    0000 0000 1111 0000 0000 0000    piece               0xf000
    0000 1111 0000 0000 0000 0000    promoted piece      0xf0000
    0001 0000 0000 0000 0000 0000    capture flag        0x100000
    0010 0000 0000 0000 0000 0000    double push flag    0x200000
    0100 0000 0000 0000 0000 0000    enpassant flag      0x400000
    1000 0000 0000 0000 0000 0000    castling flag       0x800000
*/


// add move to the move list
static inline void add_move(moves *move_list, int move)
{
    // strore move
    move_list->moves[move_list->count] = move;
    
    // increment move count
    move_list->count++;
}

// print move (for UCI purposes)
void print_move(int move)
{
    if (get_move_promoted(move))
        printf("%s%s%c", square_to_coordinates[get_move_source(move)],
                           square_to_coordinates[get_move_target(move)],
                           promoted_pieces[get_move_promoted(move)]);
    else
        printf("%s%s", square_to_coordinates[get_move_source(move)],
                           square_to_coordinates[get_move_target(move)]);
}


// print move list
void print_move_list(moves *move_list)
{
    // do nothing on empty move list
    if (!move_list->count)
    {
        printf("\n     No move in the move list!\n");
        return;
    }
    
    printf("\n     move    piece     capture   double    enpass    castling\n\n");
    
    // loop over moves within a move list
    for (int move_count = 0; move_count < move_list->count; move_count++)
    {
        // init move
        int move = move_list->moves[move_count];
        
        #ifdef WIN64
            // print move
            printf("      %s%s%c   %c         %d         %d         %d         %d\n", square_to_coordinates[get_move_source(move)],
                                                                                  square_to_coordinates[get_move_target(move)],
                                                                                  get_move_promoted(move) ? promoted_pieces[get_move_promoted(move)] : ' ',
                                                                                  ascii_pieces[get_move_piece(move)],
                                                                                  get_move_capture(move) ? 1 : 0,
                                                                                  get_move_double(move) ? 1 : 0,
                                                                                  get_move_enpassant(move) ? 1 : 0,
                                                                                  get_move_castling(move) ? 1 : 0);
        #else
            // print move
            printf("     %s%s%c   %s         %d         %d         %d         %d\n", square_to_coordinates[get_move_source(move)],
                                                                                  square_to_coordinates[get_move_target(move)],
                                                                                  get_move_promoted(move) ? promoted_pieces[get_move_promoted(move)] : ' ',
                                                                                  unicode_pieces[get_move_piece(move)],
                                                                                  get_move_capture(move) ? 1 : 0,
                                                                                  get_move_double(move) ? 1 : 0,
                                                                                  get_move_enpassant(move) ? 1 : 0,
                                                                                  get_move_castling(move) ? 1 : 0);
        #endif
        
    }
    
    // print total number of moves
    printf("\n\n     Total number of moves: %d\n\n", move_list->count);

}

// preserve board state
#define copy_board()                                                      \
    U64 bitboards_copy[12], occupancies_copy[3];                          \
    int side_copy, enpassant_copy, castle_copy;                           \
    memcpy(bitboards_copy, bitboards, 96);                                \
    memcpy(occupancies_copy, occupancies, 24);                            \
    side_copy = side, enpassant_copy = enpassant, castle_copy = castle;   \
    U64 hash_key_copy = hash_key;                                         \

// restore board state
#define take_back()                                                       \
    memcpy(bitboards, bitboards_copy, 96);                                \
    memcpy(occupancies, occupancies_copy, 24);                            \
    side = side_copy, enpassant = enpassant_copy, castle = castle_copy;   \
    hash_key = hash_key_copy;                                             \

// move types
enum { all_moves, only_captures };

/*
                           castling   move     in      in
                              right update     binary  decimal

 king & rooks didn't move:     1111 & 1111  =  1111    15

        white king  moved:     1111 & 1100  =  1100    12
  white king's rook moved:     1111 & 1110  =  1110    14
 white queen's rook moved:     1111 & 1101  =  1101    13
     
         black king moved:     1111 & 0011  =  1011    3
  black king's rook moved:     1111 & 1011  =  1011    11
 black queen's rook moved:     1111 & 0111  =  0111    7

*/

// castling rights update constants
const int castling_rights[64] = {
     7, 15, 15, 15,  3, 15, 15, 11,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    13, 15, 15, 15, 12, 15, 15, 14
};


// make move on chess board
static inline int make_move(int move, int move_flag)
{
    // quiet moves
    if (move_flag == all_moves)
    {
        // preserve board state
        copy_board();
        
        // parse move
        int source_square = get_move_source(move);
        int target_square = get_move_target(move);
        int piece = get_move_piece(move);
        int promoted_piece = get_move_promoted(move);
        int capture = get_move_capture(move);
        int double_push = get_move_double(move);
        int enpass = get_move_enpassant(move);
        int castling = get_move_castling(move);
        
        // move piece
        pop_bit(bitboards[piece], source_square);
        set_bit(bitboards[piece], target_square);
        
        // hash piece
        hash_key ^= piece_keys[piece][source_square]; // remove piece from source square in hash key
        hash_key ^= piece_keys[piece][target_square]; // set piece to the target square in hash key
        
        // handling capture moves
        if (capture)
        {
            // pick up bitboard piece index ranges depending on side
            int start_piece, end_piece;
            
            // white to move
            if (side == white)
            {
                start_piece = p;
                end_piece = k;
            }
            
            // black to move
            else
            {
                start_piece = P;
                end_piece = K;
            }
            
            // loop over bitboards opposite to the current side to move
            for (int bb_piece = start_piece; bb_piece <= end_piece; bb_piece++)
            {
                // if there's a piece on the target square
                if (get_bit(bitboards[bb_piece], target_square))
                {
                    // remove it from corresponding bitboard
                    pop_bit(bitboards[bb_piece], target_square);
                    
                    // remove the piece from hash key
                    hash_key ^= piece_keys[bb_piece][target_square];
                    break;
                }
            }
        }
        
        // handle pawn promotions
        if (promoted_piece)
        {
            // erase the pawn from the target square
            //pop_bit(bitboards[(side == white) ? P : p], target_square);
            
            
            // white to move
            if (side == white)
            {
                // erase the pawn from the target square
                pop_bit(bitboards[P], target_square);
                
                // remove pawn from hash key
                hash_key ^= piece_keys[P][target_square];
            }
            
            // black to move
            else
            {
                // erase the pawn from the target square
                pop_bit(bitboards[p], target_square);
                
                // remove pawn from hash key
                hash_key ^= piece_keys[p][target_square];
            }
            
            // set up promoted piece on chess board
            set_bit(bitboards[promoted_piece], target_square);
            
            // add promoted piece into the hash key
            hash_key ^= piece_keys[promoted_piece][target_square];
        }
        
        // handle enpassant captures
        if (enpass)
        {
            // erase the pawn depending on side to move
            (side == white) ? pop_bit(bitboards[p], target_square + 8) :
                              pop_bit(bitboards[P], target_square - 8);
                              
            // white to move
            if (side == white)
            {
                // remove captured pawn
                pop_bit(bitboards[p], target_square + 8);
                
                // remove pawn from hash key
                hash_key ^= piece_keys[p][target_square + 8];
            }
            
            // black to move
            else
            {
                // remove captured pawn
                pop_bit(bitboards[P], target_square - 8);
                
                // remove pawn from hash key
                hash_key ^= piece_keys[P][target_square - 8];
            }
        }
        
        // hash enpassant if available (remove enpassant square from hash key )
        if (enpassant != no_sq) hash_key ^= enpassant_keys[enpassant];
        
        // reset enpassant square
        enpassant = no_sq;
        
        // handle double pawn push
        if (double_push)
        {
            // set enpassant aquare depending on side to move
            //(side == white) ? (enpassant = target_square + 8) :
            //                  (enpassant = target_square - 8);
                              
            // white to move
            if (side == white)
            {
                // set enpassant square
                enpassant = target_square + 8;
                
                // hash enpassant
                hash_key ^= enpassant_keys[target_square + 8];
            }
            
            // black to move
            else
            {
                // set enpassant square
                enpassant = target_square - 8;
                
                // hash enpassant
                hash_key ^= enpassant_keys[target_square - 8];
            }
        }
        
        // handle castling moves
        if (castling)
        {
            // switch target square
            switch (target_square)
            {
                // white castles king side
                case (g1):
                    // move H rook
                    pop_bit(bitboards[R], h1);
                    set_bit(bitboards[R], f1);
                    
                    // hash rook
                    hash_key ^= piece_keys[R][h1];  // remove rook from h1 from hash key
                    hash_key ^= piece_keys[R][f1];  // put rook on f1 into a hash key
                    break;
                
                // white castles queen side
                case (c1):
                    // move A rook
                    pop_bit(bitboards[R], a1);
                    set_bit(bitboards[R], d1);
                    
                    // hash rook
                    hash_key ^= piece_keys[R][a1];  // remove rook from a1 from hash key
                    hash_key ^= piece_keys[R][d1];  // put rook on d1 into a hash key
                    break;
                
                // black castles king side
                case (g8):
                    // move H rook
                    pop_bit(bitboards[r], h8);
                    set_bit(bitboards[r], f8);
                    
                    // hash rook
                    hash_key ^= piece_keys[r][h8];  // remove rook from h8 from hash key
                    hash_key ^= piece_keys[r][f8];  // put rook on f8 into a hash key
                    break;
                
                // black castles queen side
                case (c8):
                    // move A rook
                    pop_bit(bitboards[r], a8);
                    set_bit(bitboards[r], d8);
                    
                    // hash rook
                    hash_key ^= piece_keys[r][a8];  // remove rook from a8 from hash key
                    hash_key ^= piece_keys[r][d8];  // put rook on d8 into a hash key
                    break;
            }
        }
        
        // hash castling
        hash_key ^= castle_keys[castle];
        
        // update castling rights
        castle &= castling_rights[source_square];
        castle &= castling_rights[target_square];

        // hash castling
        hash_key ^= castle_keys[castle];
        
        // reset occupancies
        memset(occupancies, 0ULL, 24);
        
        // loop over white pieces bitboards
        for (int bb_piece = P; bb_piece <= K; bb_piece++)
            // update white occupancies
            occupancies[white] |= bitboards[bb_piece];

        // loop over black pieces bitboards
        for (int bb_piece = p; bb_piece <= k; bb_piece++)
            // update black occupancies
            occupancies[black] |= bitboards[bb_piece];

        // update both sides occupancies
        occupancies[both] |= occupancies[white];
        occupancies[both] |= occupancies[black];
        
        // change side
        side ^= 1;
        
        // hash side
        hash_key ^= side_key;
        
        //
        // ====== debug hash key incremental update ======= //
        //
        
        // build hash key for the updated position (after move is made) from scratch
        /*U64 hash_from_scratch = generate_hash_key();
        
        // in case if hash key built from scratch doesn't match
        // the one that was incrementally updated we interrupt execution
        if (hash_key != hash_from_scratch)
        {
            printf("\n\nMake move\n");
            printf("move: "); print_move(move);
            print_board();
            printf("hash key should be: %llx\n", hash_from_scratch);
            getchar();
        }*/
        
        
        // make sure that king has not been exposed into a check
        if (is_square_attacked((side == white) ? get_ls1b_index(bitboards[k]) : get_ls1b_index(bitboards[K]), side))
        {
            // take move back
            take_back();
            
            // return illegal move
            return 0;
        }
        
        //
        else
            // return legal move
            return 1;
            
            
    }
    
    // capture moves
    else
    {
        // make sure move is the capture
        if (get_move_capture(move))
            make_move(move, all_moves);
        
        // otherwise the move is not a capture
        else
            // don't make it
            return 0;
    }
}

// generate all moves
static inline void generate_moves(moves *move_list)
{
    // init move count
    move_list->count = 0;

    // define source & target squares
    int source_square, target_square;
    
    // define current piece's bitboard copy & it's attacks
    U64 bitboard, attacks;
    
    // loop over all the bitboards
    for (int piece = P; piece <= k; piece++)
    {
        // init piece bitboard copy
        bitboard = bitboards[piece];
        
        // generate white pawns & white king castling moves
        if (side == white)
        {
            // pick up white pawn bitboards index
            if (piece == P)
            {
                // loop over white pawns within white pawn bitboard
                while (bitboard)
                {
                    // init source square
                    source_square = get_ls1b_index(bitboard);
                    
                    // init target square
                    target_square = source_square - 8;
                    
                    // generate quiet pawn moves
                    if (!(target_square < a8) && !get_bit(occupancies[both], target_square))
                    {
                        // pawn promotion
                        if (source_square >= a7 && source_square <= h7)
                        {                            
                            add_move(move_list, encode_move(source_square, target_square, piece, Q, 0, 0, 0, 0));
                            add_move(move_list, encode_move(source_square, target_square, piece, R, 0, 0, 0, 0));
                            add_move(move_list, encode_move(source_square, target_square, piece, B, 0, 0, 0, 0));
                            add_move(move_list, encode_move(source_square, target_square, piece, N, 0, 0, 0, 0));
                        }
                        
                        else
                        {
                            // one square ahead pawn move
                            add_move(move_list, encode_move(source_square, target_square, piece, 0, 0, 0, 0, 0));
                            
                            // two squares ahead pawn move
                            if ((source_square >= a2 && source_square <= h2) && !get_bit(occupancies[both], target_square - 8))
                                add_move(move_list, encode_move(source_square, target_square - 8, piece, 0, 0, 1, 0, 0));
                        }
                    }
                    
                    // init pawn attacks bitboard
                    attacks = pawn_attacks[side][source_square] & occupancies[black];
                    
                    // generate pawn captures
                    while (attacks)
                    {
                        // init target square
                        target_square = get_ls1b_index(attacks);
                        
                        // pawn promotion
                        if (source_square >= a7 && source_square <= h7)
                        {
                            add_move(move_list, encode_move(source_square, target_square, piece, Q, 1, 0, 0, 0));
                            add_move(move_list, encode_move(source_square, target_square, piece, R, 1, 0, 0, 0));
                            add_move(move_list, encode_move(source_square, target_square, piece, B, 1, 0, 0, 0));
                            add_move(move_list, encode_move(source_square, target_square, piece, N, 1, 0, 0, 0));
                        }
                        
                        else
                            // one square ahead pawn move
                            add_move(move_list, encode_move(source_square, target_square, piece, 0, 1, 0, 0, 0));
                        
                        // pop ls1b of the pawn attacks
                        pop_bit(attacks, target_square);
                    }
                    
                    // generate enpassant captures
                    if (enpassant != no_sq)
                    {
                        // lookup pawn attacks and bitwise AND with enpassant square (bit)
                        U64 enpassant_attacks = pawn_attacks[side][source_square] & (1ULL << enpassant);
                        
                        // make sure enpassant capture available
                        if (enpassant_attacks)
                        {
                            // init enpassant capture target square
                            int target_enpassant = get_ls1b_index(enpassant_attacks);
                            add_move(move_list, encode_move(source_square, target_enpassant, piece, 0, 1, 0, 1, 0));
                        }
                    }
                    
                    // pop ls1b from piece bitboard copy
                    pop_bit(bitboard, source_square);
                }
            }
            
            // castling moves
            if (piece == K)
            {
                // king side castling is available
                if (castle & wk)
                {
                    // make sure square between king and king's rook are empty
                    if (!get_bit(occupancies[both], f1) && !get_bit(occupancies[both], g1))
                    {
                        // make sure king and the f1 squares are not under attacks
                        if (!is_square_attacked(e1, black) && !is_square_attacked(f1, black))
                            add_move(move_list, encode_move(e1, g1, piece, 0, 0, 0, 0, 1));
                    }
                }
                
                // queen side castling is available
                if (castle & wq)
                {
                    // make sure square between king and queen's rook are empty
                    if (!get_bit(occupancies[both], d1) && !get_bit(occupancies[both], c1) && !get_bit(occupancies[both], b1))
                    {
                        // make sure king and the d1 squares are not under attacks
                        if (!is_square_attacked(e1, black) && !is_square_attacked(d1, black))
                            add_move(move_list, encode_move(e1, c1, piece, 0, 0, 0, 0, 1));
                    }
                }
            }
        }
        
        // generate black pawns & black king castling moves
        else
        {
            // pick up black pawn bitboards index
            if (piece == p)
            {
                // loop over white pawns within white pawn bitboard
                while (bitboard)
                {
                    // init source square
                    source_square = get_ls1b_index(bitboard);
                    
                    // init target square
                    target_square = source_square + 8;
                    
                    // generate quiet pawn moves
                    if (!(target_square > h1) && !get_bit(occupancies[both], target_square))
                    {
                        // pawn promotion
                        if (source_square >= a2 && source_square <= h2)
                        {
                            add_move(move_list, encode_move(source_square, target_square, piece, q, 0, 0, 0, 0));
                            add_move(move_list, encode_move(source_square, target_square, piece, r, 0, 0, 0, 0));
                            add_move(move_list, encode_move(source_square, target_square, piece, b, 0, 0, 0, 0));
                            add_move(move_list, encode_move(source_square, target_square, piece, n, 0, 0, 0, 0));
                        }
                        
                        else
                        {
                            // one square ahead pawn move
                            add_move(move_list, encode_move(source_square, target_square, piece, 0, 0, 0, 0, 0));
                            
                            // two squares ahead pawn move
                            if ((source_square >= a7 && source_square <= h7) && !get_bit(occupancies[both], target_square + 8))
                                add_move(move_list, encode_move(source_square, target_square + 8, piece, 0, 0, 1, 0, 0));
                        }
                    }
                    
                    // init pawn attacks bitboard
                    attacks = pawn_attacks[side][source_square] & occupancies[white];
                    
                    // generate pawn captures
                    while (attacks)
                    {
                        // init target square
                        target_square = get_ls1b_index(attacks);
                        
                        // pawn promotion
                        if (source_square >= a2 && source_square <= h2)
                        {
                            add_move(move_list, encode_move(source_square, target_square, piece, q, 1, 0, 0, 0));
                            add_move(move_list, encode_move(source_square, target_square, piece, r, 1, 0, 0, 0));
                            add_move(move_list, encode_move(source_square, target_square, piece, b, 1, 0, 0, 0));
                            add_move(move_list, encode_move(source_square, target_square, piece, n, 1, 0, 0, 0));
                        }
                        
                        else
                            // one square ahead pawn move
                            add_move(move_list, encode_move(source_square, target_square, piece, 0, 1, 0, 0, 0));
                        
                        // pop ls1b of the pawn attacks
                        pop_bit(attacks, target_square);
                    }
                    
                    // generate enpassant captures
                    if (enpassant != no_sq)
                    {
                        // lookup pawn attacks and bitwise AND with enpassant square (bit)
                        U64 enpassant_attacks = pawn_attacks[side][source_square] & (1ULL << enpassant);
                        
                        // make sure enpassant capture available
                        if (enpassant_attacks)
                        {
                            // init enpassant capture target square
                            int target_enpassant = get_ls1b_index(enpassant_attacks);
                            add_move(move_list, encode_move(source_square, target_enpassant, piece, 0, 1, 0, 1, 0));
                        }
                    }
                    
                    // pop ls1b from piece bitboard copy
                    pop_bit(bitboard, source_square);
                }
            }
            
            // castling moves
            if (piece == k)
            {
                // king side castling is available
                if (castle & bk)
                {
                    // make sure square between king and king's rook are empty
                    if (!get_bit(occupancies[both], f8) && !get_bit(occupancies[both], g8))
                    {
                        // make sure king and the f8 squares are not under attacks
                        if (!is_square_attacked(e8, white) && !is_square_attacked(f8, white))
                            add_move(move_list, encode_move(e8, g8, piece, 0, 0, 0, 0, 1));
                    }
                }
                
                // queen side castling is available
                if (castle & bq)
                {
                    // make sure square between king and queen's rook are empty
                    if (!get_bit(occupancies[both], d8) && !get_bit(occupancies[both], c8) && !get_bit(occupancies[both], b8))
                    {
                        // make sure king and the d8 squares are not under attacks
                        if (!is_square_attacked(e8, white) && !is_square_attacked(d8, white))
                            add_move(move_list, encode_move(e8, c8, piece, 0, 0, 0, 0, 1));
                    }
                }
            }
        }
        
        // genarate knight moves
        if ((side == white) ? piece == N : piece == n)
        {
            // loop over source squares of piece bitboard copy
            while (bitboard)
            {
                // init source square
                source_square = get_ls1b_index(bitboard);
                
                // init piece attacks in order to get set of target squares
                attacks = knight_attacks[source_square] & ((side == white) ? ~occupancies[white] : ~occupancies[black]);
                
                // loop over target squares available from generated attacks
                while (attacks)
                {
                    // init target square
                    target_square = get_ls1b_index(attacks);    
                    
                    // quiet move
                    if (!get_bit(((side == white) ? occupancies[black] : occupancies[white]), target_square))
                        add_move(move_list, encode_move(source_square, target_square, piece, 0, 0, 0, 0, 0));
                    
                    else
                        // capture move
                        add_move(move_list, encode_move(source_square, target_square, piece, 0, 1, 0, 0, 0));
                    
                    // pop ls1b in current attacks set
                    pop_bit(attacks, target_square);
                }
                
                
                // pop ls1b of the current piece bitboard copy
                pop_bit(bitboard, source_square);
            }
        }
        
        // generate bishop moves
        if ((side == white) ? piece == B : piece == b)
        {
            // loop over source squares of piece bitboard copy
            while (bitboard)
            {
                // init source square
                source_square = get_ls1b_index(bitboard);
                
                // init piece attacks in order to get set of target squares
                attacks = get_bishop_attacks(source_square, occupancies[both]) & ((side == white) ? ~occupancies[white] : ~occupancies[black]);
                
                // loop over target squares available from generated attacks
                while (attacks)
                {
                    // init target square
                    target_square = get_ls1b_index(attacks);    
                    
                    // quiet move
                    if (!get_bit(((side == white) ? occupancies[black] : occupancies[white]), target_square))
                        add_move(move_list, encode_move(source_square, target_square, piece, 0, 0, 0, 0, 0));
                    
                    else
                        // capture move
                        add_move(move_list, encode_move(source_square, target_square, piece, 0, 1, 0, 0, 0));
                    
                    // pop ls1b in current attacks set
                    pop_bit(attacks, target_square);
                }
                
                
                // pop ls1b of the current piece bitboard copy
                pop_bit(bitboard, source_square);
            }
        }
        
        // generate rook moves
        if ((side == white) ? piece == R : piece == r)
        {
            // loop over source squares of piece bitboard copy
            while (bitboard)
            {
                // init source square
                source_square = get_ls1b_index(bitboard);
                
                // init piece attacks in order to get set of target squares
                attacks = get_rook_attacks(source_square, occupancies[both]) & ((side == white) ? ~occupancies[white] : ~occupancies[black]);
                
                // loop over target squares available from generated attacks
                while (attacks)
                {
                    // init target square
                    target_square = get_ls1b_index(attacks);    
                    
                    // quiet move
                    if (!get_bit(((side == white) ? occupancies[black] : occupancies[white]), target_square))
                        add_move(move_list, encode_move(source_square, target_square, piece, 0, 0, 0, 0, 0));
                    
                    else
                        // capture move
                        add_move(move_list, encode_move(source_square, target_square, piece, 0, 1, 0, 0, 0));
                    
                    // pop ls1b in current attacks set
                    pop_bit(attacks, target_square);
                }
                
                
                // pop ls1b of the current piece bitboard copy
                pop_bit(bitboard, source_square);
            }
        }
        
        // generate queen moves
        if ((side == white) ? piece == Q : piece == q)
        {
            // loop over source squares of piece bitboard copy
            while (bitboard)
            {
                // init source square
                source_square = get_ls1b_index(bitboard);
                
                // init piece attacks in order to get set of target squares
                attacks = get_queen_attacks(source_square, occupancies[both]) & ((side == white) ? ~occupancies[white] : ~occupancies[black]);
                
                // loop over target squares available from generated attacks
                while (attacks)
                {
                    // init target square
                    target_square = get_ls1b_index(attacks);    
                    
                    // quiet move
                    if (!get_bit(((side == white) ? occupancies[black] : occupancies[white]), target_square))
                        add_move(move_list, encode_move(source_square, target_square, piece, 0, 0, 0, 0, 0));
                    
                    else
                        // capture move
                        add_move(move_list, encode_move(source_square, target_square, piece, 0, 1, 0, 0, 0));
                    
                    // pop ls1b in current attacks set
                    pop_bit(attacks, target_square);
                }
                
                
                // pop ls1b of the current piece bitboard copy
                pop_bit(bitboard, source_square);
            }
        }

        // generate king moves
        if ((side == white) ? piece == K : piece == k)
        {
            // loop over source squares of piece bitboard copy
            while (bitboard)
            {
                // init source square
                source_square = get_ls1b_index(bitboard);
                
                // init piece attacks in order to get set of target squares
                attacks = king_attacks[source_square] & ((side == white) ? ~occupancies[white] : ~occupancies[black]);
                
                // loop over target squares available from generated attacks
                while (attacks)
                {
                    // init target square
                    target_square = get_ls1b_index(attacks);    
                    
                    // quiet move
                    if (!get_bit(((side == white) ? occupancies[black] : occupancies[white]), target_square))
                        add_move(move_list, encode_move(source_square, target_square, piece, 0, 0, 0, 0, 0));
                    
                    else
                        // capture move
                        add_move(move_list, encode_move(source_square, target_square, piece, 0, 1, 0, 0, 0));
                    
                    // pop ls1b in current attacks set
                    pop_bit(attacks, target_square);
                }

                // pop ls1b of the current piece bitboard copy
                pop_bit(bitboard, source_square);
            }
        }
    }
}


/**********************************\
 ==================================
 
               Perft
 
 ==================================
\**********************************/

// leaf nodes (number of positions reached during the test of the move generator at a given depth)
long nodes;

// perft driver
static inline void perft_driver(int depth)
{
    // reccursion escape condition
    if (depth == 0)
    {
        // increment nodes count (count reached positions)
        nodes++;
        return;
    }
    
    // create move list instance
    moves move_list[1];
    
    // generate moves
    generate_moves(move_list);
    
        // loop over generated moves
    for (int move_count = 0; move_count < move_list->count; move_count++)
    {   
        // preserve board state
        copy_board();
        
        // make move
        if (!make_move(move_list->moves[move_count], all_moves))
            // skip to the next move
            continue;
        
        // call perft driver recursively
        perft_driver(depth - 1);
        
        // take back
        take_back();
        
        
        // build hash key for the updated position (after move is made) from scratch
        /*U64 hash_from_scratch = generate_hash_key();
        
        // in case if hash key built from scratch doesn't match
        // the one that was incrementally updated we interrupt execution
        if (hash_key != hash_from_scratch)
        {
            printf("\n\nTake back\n");
            printf("move: "); print_move(move_list->moves[move_count]);
            print_board();
            printf("hash key should be: %llx\n", hash_from_scratch);
            getchar();
        }*/
    }
}

// perft test
void perft_test(int depth)
{
    printf("\n     Performance test\n\n");
    
    // create move list instance
    moves move_list[1];
    
    // generate moves
    generate_moves(move_list);
    
    // init start time
    long start = get_time_ms();
    
    // loop over generated moves
    for (int move_count = 0; move_count < move_list->count; move_count++)
    {   
        // preserve board state
        copy_board();
        
        // make move
        if (!make_move(move_list->moves[move_count], all_moves))
            // skip to the next move
            continue;
        
        // cummulative nodes
        long cummulative_nodes = nodes;
        
        // call perft driver recursively
        perft_driver(depth - 1);
        
        // old nodes
        long old_nodes = nodes - cummulative_nodes;
        
        // take back
        take_back();
        
        // print move
        printf("     move: %s%s%c  nodes: %ld\n", square_to_coordinates[get_move_source(move_list->moves[move_count])],
                                                 square_to_coordinates[get_move_target(move_list->moves[move_count])],
                                                 get_move_promoted(move_list->moves[move_count]) ? promoted_pieces[get_move_promoted(move_list->moves[move_count])] : ' ',
                                                 old_nodes);
    }
    
    // print results
    printf("\n    Depth: %d\n", depth);
    printf("    Nodes: %ld\n", nodes);
    printf("     Time: %ld\n\n", get_time_ms() - start);
}


/**********************************\
 ==================================
 
             Evaluation
 
 ==================================
\**********************************/


// file masks [square]
U64 file_masks[64];

// rank masks [square]
U64 rank_masks[64];

// isolated pawn masks [square]
U64 isolated_masks[64];

// white passed pawn masks [square]
U64 white_passed_masks[64];

// black passed pawn masks [square]
U64 black_passed_masks[64];

// set file or rank mask
U64 set_file_rank_mask(int file_number, int rank_number)
{
    // file or rank mask
    U64 mask = 0ULL;
    
    // loop over ranks
    for (int rank = 0; rank < 8; rank++)
    {
        // loop over files
        for (int file = 0; file < 8; file++)
        {
            // init square
            int square = rank * 8 + file;
            
            if (file_number != -1)
            {
                // on file match
                if (file == file_number)
                    // set bit on mask
                    mask |= set_bit(mask, square);
            }
            
            else if (rank_number != -1)
            {
                // on rank match
                if (rank == rank_number)
                    // set bit on mask
                    mask |= set_bit(mask, square);
            }
        }
    }
    
    // return mask
    return mask;
}

// init evaluation masks
void init_evaluation_masks()
{
    /******** Init file masks ********/
    
    // loop over ranks
    for (int rank = 0; rank < 8; rank++)
    {
        // loop over files
        for (int file = 0; file < 8; file++)
        {
            // init square
            int square = rank * 8 + file;
            
            // init file mask for a current square
            file_masks[square] |= set_file_rank_mask(file, -1);
        }
    }
    
    /******** Init rank masks ********/
    
    // loop over ranks
    for (int rank = 0; rank < 8; rank++)
    {
        // loop over files
        for (int file = 0; file < 8; file++)
        {
            // init square
            int square = rank * 8 + file;
            
            // init file mask for a current square
            rank_masks[square] |= set_file_rank_mask(-1, rank);
        }
    }
    
    /******** Init isolated masks ********/
    
    // loop over ranks
    for (int rank = 0; rank < 8; rank++)
    {
        // loop over files
        for (int file = 0; file < 8; file++)
        {
            // init square
            int square = rank * 8 + file;
            
            // init file mask for a current square
            isolated_masks[square] |= set_file_rank_mask(file - 1, -1);
            isolated_masks[square] |= set_file_rank_mask(file + 1, -1);
        }
    }
    
    /******** White passed masks ********/
    
    // loop over ranks
    for (int rank = 0; rank < 8; rank++)
    {
        // loop over files
        for (int file = 0; file < 8; file++)
        {
            // init square
            int square = rank * 8 + file;
            
            // init file mask for a current square
            white_passed_masks[square] |= set_file_rank_mask(file - 1, -1);
            white_passed_masks[square] |= set_file_rank_mask(file, -1);
            white_passed_masks[square] |= set_file_rank_mask(file + 1, -1);
            
            // loop over redudant ranks
            for (int i = 0; i < (8 - rank); i++)
                // reset redudant bits 
                white_passed_masks[square] &= ~rank_masks[(7 - i) * 8 + file];
        }
    }
    
    /******** Black passed masks ********/
    
    // loop over ranks
    for (int rank = 0; rank < 8; rank++)
    {
        // loop over files
        for (int file = 0; file < 8; file++)
        {
            // init square
            int square = rank * 8 + file;
            
            // init file mask for a current square
            black_passed_masks[square] |= set_file_rank_mask(file - 1, -1);
            black_passed_masks[square] |= set_file_rank_mask(file, -1);
            black_passed_masks[square] |= set_file_rank_mask(file + 1, -1);
            
            // loop over redudant ranks
            for (int i = 0; i < rank + 1; i++)
                // reset redudant bits 
                black_passed_masks[square] &= ~rank_masks[i * 8 + file];
            
        }
    }
}

// extract rank from a square [square]
const int get_rank[64] =
{
    7, 7, 7, 7, 7, 7, 7, 7,
    6, 6, 6, 6, 6, 6, 6, 6,
    5, 5, 5, 5, 5, 5, 5, 5,
    4, 4, 4, 4, 4, 4, 4, 4,
    3, 3, 3, 3, 3, 3, 3, 3,
    2, 2, 2, 2, 2, 2, 2, 2,
    1, 1, 1, 1, 1, 1, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0
};


// double pawns penalty
const int double_pawn_penalty_opening = -5;
const int double_pawn_penalty_endgame = -10;

// isolated pawn penalty
const int isolated_pawn_penalty_opening = -5;
const int isolated_pawn_penalty_endgame = -10;

// passed pawn bonus
const int passed_pawn_bonus[8] = { 0, 10, 30, 50, 75, 100, 150, 200 }; 

// semi open file score
const int semi_open_file_score = 10;

// open file score
const int open_file_score = 15;

// mobility units (values from engine Fruit reloaded)
static const int bishop_unit = 4;
static const int queen_unit = 9;

// mobility bonuses (values from engine Fruit reloaded)
static const int bishop_mobility_opening = 5;
static const int bishop_mobility_endgame = 5;
static const int queen_mobility_opening = 1;
static const int queen_mobility_endgame = 2;

int mg_value[6] = { 82, 337, 365, 477, 1025,  0};
int eg_value[6] = { 94, 281, 297, 512,  936,  0};

/* piece/sq tables */
/* values from Rofchade: http://www.talkchess.com/forum3/viewtopic.php?f=2&t=68311&start=19 */

int mg_pawn_table[64] = {
      0,   0,   0,   0,   0,   0,  0,   0,
     98, 134,  61,  95,  68, 126, 34, -11,
     -6,   7,  26,  31,  65,  56, 25, -20,
    -14,  13,   6,  21,  23,  12, 17, -23,
    -27,  -2,  -5,  12,  17,   6, 10, -25,
    -26,  -4,  -4, -10,   3,   3, 33, -12,
    -35,  -1, -20, -23, -15,  24, 38, -22,
      0,   0,   0,   0,   0,   0,  0,   0,
};

int eg_pawn_table[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
    178, 173, 158, 134, 147, 132, 165, 187,
     94, 100,  85,  67,  56,  53,  82,  84,
     32,  24,  13,   5,  -2,   4,  17,  17,
     13,   9,  -3,  -7,  -7,  -8,   3,  -1,
      4,   7,  -6,   1,   0,  -5,  -1,  -8,
     13,   8,   8,  10,  13,   0,   2,  -7,
      0,   0,   0,   0,   0,   0,   0,   0,
};

int mg_knight_table[64] = {
    -167, -89, -34, -49,  61, -97, -15, -107,
     -73, -41,  72,  36,  23,  62,   7,  -17,
     -47,  60,  37,  65,  84, 129,  73,   44,
      -9,  17,  19,  53,  37,  69,  18,   22,
     -13,   4,  16,  13,  28,  19,  21,   -8,
     -23,  -9,  12,  10,  19,  17,  25,  -16,
     -29, -53, -12,  -3,  -1,  18, -14,  -19,
    -105, -21, -58, -33, -17, -28, -19,  -23,
};

int eg_knight_table[64] = {
    -58, -38, -13, -28, -31, -27, -63, -99,
    -25,  -8, -25,  -2,  -9, -25, -24, -52,
    -24, -20,  10,   9,  -1,  -9, -19, -41,
    -17,   3,  22,  22,  22,  11,   8, -18,
    -18,  -6,  16,  25,  16,  17,   4, -18,
    -23,  -3,  -1,  15,  10,  -3, -20, -22,
    -42, -20, -10,  -5,  -2, -20, -23, -44,
    -29, -51, -23, -15, -22, -18, -50, -64,
};

int mg_bishop_table[64] = {
    -29,   4, -82, -37, -25, -42,   7,  -8,
    -26,  16, -18, -13,  30,  59,  18, -47,
    -16,  37,  43,  40,  35,  50,  37,  -2,
     -4,   5,  19,  50,  37,  37,   7,  -2,
     -6,  13,  13,  26,  34,  12,  10,   4,
      0,  15,  15,  15,  14,  27,  18,  10,
      4,  15,  16,   0,   7,  21,  33,   1,
    -33,  -3, -14, -21, -13, -12, -39, -21,
};

int eg_bishop_table[64] = {
    -14, -21, -11,  -8, -7,  -9, -17, -24,
     -8,  -4,   7, -12, -3, -13,  -4, -14,
      2,  -8,   0,  -1, -2,   6,   0,   4,
     -3,   9,  12,   9, 14,  10,   3,   2,
     -6,   3,  13,  19,  7,  10,  -3,  -9,
    -12,  -3,   8,  10, 13,   3,  -7, -15,
    -14, -18,  -7,  -1,  4,  -9, -15, -27,
    -23,  -9, -23,  -5, -9, -16,  -5, -17,
};

int mg_rook_table[64] = {
     32,  42,  32,  51, 63,  9,  31,  43,
     27,  32,  58,  62, 80, 67,  26,  44,
     -5,  19,  26,  36, 17, 45,  61,  16,
    -24, -11,   7,  26, 24, 35,  -8, -20,
    -36, -26, -12,  -1,  9, -7,   6, -23,
    -45, -25, -16, -17,  3,  0,  -5, -33,
    -44, -16, -20,  -9, -1, 11,  -6, -71,
    -19, -13,   1,  17, 16,  7, -37, -26,
};

int eg_rook_table[64] = {
    13, 10, 18, 15, 12,  12,   8,   5,
    11, 13, 13, 11, -3,   3,   8,   3,
     7,  7,  7,  5,  4,  -3,  -5,  -3,
     4,  3, 13,  1,  2,   1,  -1,   2,
     3,  5,  8,  4, -5,  -6,  -8, -11,
    -4,  0, -5, -1, -7, -12,  -8, -16,
    -6, -6,  0,  2, -9,  -9, -11,  -3,
    -9,  2,  3, -1, -5, -13,   4, -20,
};

int mg_queen_table[64] = {
    -28,   0,  29,  12,  59,  44,  43,  45,
    -24, -39,  -5,   1, -16,  57,  28,  54,
    -13, -17,   7,   8,  29,  56,  47,  57,
    -27, -27, -16, -16,  -1,  17,  -2,   1,
     -9, -26,  -9, -10,  -2,  -4,   3,  -3,
    -14,   2, -11,  -2,  -5,   2,  14,   5,
    -35,  -8,  11,   2,   8,  15,  -3,   1,
     -1, -18,  -9,  10, -15, -25, -31, -50,
};

int eg_queen_table[64] = {
     -9,  22,  22,  27,  27,  19,  10,  20,
    -17,  20,  32,  41,  58,  25,  30,   0,
    -20,   6,   9,  49,  47,  35,  19,   9,
      3,  22,  24,  45,  57,  40,  57,  36,
    -18,  28,  19,  47,  31,  34,  39,  23,
    -16, -27,  15,   6,   9,  17,  10,   5,
    -22, -23, -30, -16, -16, -23, -36, -32,
    -33, -28, -22, -43,  -5, -32, -20, -41,
};

int mg_king_table[64] = {
    -65,  23,  16, -15, -56, -34,   2,  13,
     29,  -1, -20,  -7,  -8,  -4, -38, -29,
     -9,  24,   2, -16, -20,   6,  22, -22,
    -17, -20, -12, -27, -30, -25, -14, -36,
    -49,  -1, -27, -39, -46, -44, -33, -51,
    -14, -14, -22, -46, -44, -30, -15, -27,
      1,   7,  -8, -64, -43, -16,   9,   8,
    -15,  36,  12, -54,   8, -28,  24,  14,
};

int eg_king_table[64] = {
    -74, -35, -18, -18, -11,  15,   4, -17,
    -12,  17,  14,  17,  17,  38,  23,  11,
     10,  17,  23,  15,  20,  45,  44,  13,
     -8,  22,  24,  27,  26,  33,  26,   3,
    -18,  -4,  21,  24,  27,  23,   9, -11,
    -19,  -3,  11,  21,  23,  16,   7,  -9,
    -27, -11,   4,  13,  14,   4,  -5, -17,
    -53, -34, -21, -11, -28, -14, -24, -43
};

int* mg_pesto_table[6] =
{
    mg_pawn_table,
    mg_knight_table,
    mg_bishop_table,
    mg_rook_table,
    mg_queen_table,
    mg_king_table
};

int* eg_pesto_table[6] =
{
    eg_pawn_table,
    eg_knight_table,
    eg_bishop_table,
    eg_rook_table,
    eg_queen_table,
    eg_king_table
};

int gamephaseInc[12] = {0,0,1,1,1,1,2,2,4,4,0,0};
int mg_table[12][64];
int eg_table[12][64];

void init_tables()
{
    for (int pc = P;pc < p; pc++){
        int p = PieceToPieceType[pc];
        for (int sq = 0; sq < 64; sq++) {
            mg_table[pc][sq] = mg_value[p] + mg_pesto_table[p][sq];
            eg_table[pc][sq] = eg_value[p] + eg_pesto_table[p][sq];
            mg_table[pc+6][sq] = mg_value[p] + mg_pesto_table[p][mirror_score[sq]];
            eg_table[pc+6][sq] = eg_value[p] + eg_pesto_table[p][mirror_score[sq]];
        }
    }
}

static inline int pesto_evaluate()
{
    int mg[2];
    int eg[2];
    int gamePhase = 0;

    mg[white] = 0;
    mg[black] = 0;
    eg[white] = 0;
    eg[black] = 0;
    
    U64 bitboard;
    
    // init piece & square
    int piece, square;
    
    // penalties
    int double_pawns = 0;

    for (int bb_piece = P; bb_piece <= k; bb_piece++)
    {
        // init piece bitboard copy
        bitboard = bitboards[bb_piece];
        
        
        // loop over pieces within a bitboard
        while (bitboard)
        {
            // init piece
            piece = bb_piece;
            
            // init square
            square = get_ls1b_index(bitboard);

            switch (piece)
            {
                case P:

                    mg[white] += mg_table[bb_piece][square];
                    eg[white] += eg_table[bb_piece][square];
                    gamePhase += gamephaseInc[bb_piece];

                    double_pawns = count_bits(bitboards[P] & file_masks[square]);

                    if (double_pawns > 1){
                        mg[white] += double_pawns * double_pawn_penalty_opening;
                        eg[white] += double_pawns * double_pawn_penalty_endgame;
                    }
                    if ((bitboards[P] & isolated_masks[square]) == 0){
                        mg[white] += isolated_pawn_penalty_opening;
                        eg[white] += isolated_pawn_penalty_endgame;
                    }
                    if ((white_passed_masks[square] & bitboards[p]) == 0){
                        mg[white] += passed_pawn_bonus[get_rank[square]];
                        eg[white] += passed_pawn_bonus[get_rank[square]];
                    }

                    break;

                case N: 
                    mg[white] += mg_table[bb_piece][square];
                    eg[white] += eg_table[bb_piece][square];
                    gamePhase += gamephaseInc[bb_piece]; break;
                case B: 
                    mg[white] += mg_table[bb_piece][square];
                    eg[white] += eg_table[bb_piece][square]; 
                    gamePhase += gamephaseInc[bb_piece];

                    mg[white] += (count_bits(get_bishop_attacks(square, occupancies[both])) - bishop_unit) * bishop_mobility_opening;
                    eg[white] += (count_bits(get_bishop_attacks(square, occupancies[both])) - bishop_unit) * bishop_mobility_endgame;
                    
                    break;
                
                case R:

                    mg[white] += mg_table[bb_piece][square];
                    eg[white] += eg_table[bb_piece][square];
                    gamePhase += gamephaseInc[bb_piece];
                    
                    if ((bitboards[P] & file_masks[square]) == 0){
                        // add semi open file bonus
                        mg[white] += semi_open_file_score;
                        eg[white] += semi_open_file_score;
                    }
                    
                    if (((bitboards[P] | bitboards[p]) & file_masks[square]) == 0){
                        mg[white] += open_file_score;
                        eg[white] += open_file_score;
                    }
                    
                    break;
                
                case Q:
                    mg[white] += mg_table[bb_piece][square];
                    eg[white] += eg_table[bb_piece][square];
                    gamePhase += gamephaseInc[bb_piece];

                    mg[white] += (count_bits(get_queen_attacks(square, occupancies[both])) - queen_unit) * queen_mobility_opening;
                    eg[white] += (count_bits(get_queen_attacks(square, occupancies[both])) - queen_unit) * queen_mobility_endgame;
                    break;
                case K:
                    mg[white] += mg_table[bb_piece][square];
                    eg[white] += eg_table[bb_piece][square];
                    gamePhase += gamephaseInc[bb_piece];
                    

                    if ((bitboards[P] & file_masks[square]) == 0){
                        mg[white] -= semi_open_file_score;
                        eg[white] -= semi_open_file_score;
                    }
                    
                    if (((bitboards[P] | bitboards[p]) & file_masks[square]) == 0){
                        mg[white] -= open_file_score;
                        eg[white] -= open_file_score;
                    }

                    mg[white] += count_bits(king_attacks[square] & occupancies[white]) * 5;
                    eg[white] += count_bits(king_attacks[square] & occupancies[white]) * 5;

                    break;

                case p:
                    mg[black] += mg_table[bb_piece][square];
                    eg[black] += eg_table[bb_piece][square];
                    gamePhase += gamephaseInc[bb_piece];

                    double_pawns = count_bits(bitboards[p] & file_masks[square]);

                    if (double_pawns > 1){
                        mg[black] += double_pawns * double_pawn_penalty_opening;
                        eg[black] += double_pawns * double_pawn_penalty_endgame;
                    }
                    if ((bitboards[p] & isolated_masks[square]) == 0){
                        mg[black] += isolated_pawn_penalty_opening;
                        eg[black] += isolated_pawn_penalty_endgame;
                    }
                    if ((black_passed_masks[square] & bitboards[P]) == 0){
                        mg[black] += passed_pawn_bonus[get_rank[square]];
                        eg[black] += passed_pawn_bonus[get_rank[square]];
                    }

                    break;

                case n: 
                    mg[black] += mg_table[bb_piece][square];
                    eg[black] += eg_table[bb_piece][square];
                    gamePhase += gamephaseInc[bb_piece];
                    break;
                case b: 
                    mg[black] += mg_table[bb_piece][square];
                    eg[black] += eg_table[bb_piece][square];
                    gamePhase += gamephaseInc[bb_piece];

                    mg[black] += (count_bits(get_bishop_attacks(square, occupancies[both])) - bishop_unit) * bishop_mobility_opening;
                    eg[black] += (count_bits(get_bishop_attacks(square, occupancies[both])) - bishop_unit) * bishop_mobility_endgame;
                    break;
                case r:
                    
                    mg[black] += mg_table[bb_piece][square];
                    eg[black] += eg_table[bb_piece][square];
                    gamePhase += gamephaseInc[bb_piece];
                    
                    if ((bitboards[p] & file_masks[square]) == 0){
                        // add semi open file bonus
                        mg[black] += semi_open_file_score;
                        eg[black] += semi_open_file_score;
                    }
                    
                    if (((bitboards[P] | bitboards[p]) & file_masks[square]) == 0){
                        mg[black] += open_file_score;
                        eg[black] += open_file_score;
                    }
                    
                    break;
                case q:
                    mg[black] += mg_table[bb_piece][square];
                    eg[black] += eg_table[bb_piece][square];
                    gamePhase += gamephaseInc[bb_piece];

                    mg[black] += (count_bits(get_queen_attacks(square, occupancies[both])) - queen_unit) * queen_mobility_opening;
                    eg[black] += (count_bits(get_queen_attacks(square, occupancies[both])) - queen_unit) * queen_mobility_endgame;
                    break;
                case k:
                    mg[black] += mg_table[bb_piece][square];
                    eg[black] += eg_table[bb_piece][square];
                    gamePhase += gamephaseInc[bb_piece];

                    if ((bitboards[P] & file_masks[square]) == 0){
                        mg[white] -= semi_open_file_score;
                        eg[white] -= semi_open_file_score;
                    }
                    
                    if (((bitboards[P] | bitboards[p]) & file_masks[square]) == 0){
                        mg[white] -= open_file_score;
                        eg[white] -= open_file_score;
                    }

                    mg[white] += count_bits(king_attacks[square] & occupancies[white]) * 5;
                    eg[white] += count_bits(king_attacks[square] & occupancies[white]) * 5;
                    mg[black] += count_bits(king_attacks[square] & occupancies[black]) * 5;
                    eg[black] += count_bits(king_attacks[square] & occupancies[black]) * 5;
                    break;
            }
            // pop ls1b
            pop_bit(bitboard, square);
        }
    }

    int otherside = (side == white) ? black : white;

    /* tapered eval */
    int mgScore = mg[side] - mg[otherside];
    int egScore = eg[side] - eg[otherside];
    int mgPhase = gamePhase;
    if (mgPhase > 24) mgPhase = 24; /* in case of early promotion */
    int egPhase = 24 - mgPhase;
    return (mgScore * mgPhase + egScore * egPhase) / 24;
}

// position evaluation
static inline int evaluate()
{
    // static evaluation score
    int score = 0;
    
    // current pieces bitboard copy
    U64 bitboard;
    
    // init piece & square
    int piece, square;
    
    // penalties
    int double_pawns = 0;

    int material;
    
    // loop over piece bitboards
    for (int bb_piece = P; bb_piece <= k; bb_piece++)
    {
        // init piece bitboard copy
        bitboard = bitboards[bb_piece];
        
        // loop over pieces within a bitboard
        while (bitboard)
        {
            // init piece
            piece = bb_piece;
            
            // init square
            square = get_ls1b_index(bitboard);
            
            // score material weights
            
            
            // score positional piece scores
            switch (piece)
            {
                // evaluate white pawns
                case P:
                    // positional score
                    score += pawn_score[square];
                    material += material_score[PAWN];
                    // double pawn penalty
                    //double_pawns = count_bits(bitboards[P] & file_masks[square]);
                    
                    // on double pawns (tripple, etc)
                    /*if (double_pawns > 1)
                        score += double_pawns * double_pawn_penalty;
                    
                    // on isolated pawn
                    if ((bitboards[P] & isolated_masks[square]) == 0)
                        // give an isolated pawn penalty
                        score += isolated_pawn_penalty;
                    
                    // on passed pawn
                    if ((white_passed_masks[square] & bitboards[p]) == 0)
                        // give passed pawn bonus
                        score += passed_pawn_bonus[get_rank[square]];*/

                    break;
                
                // evaluate white knights
                case N:
                    // positional score
                    score += knight_score[square];
                    material += material_score[KNIGHT];
                    break;
                
                // evaluate white bishops
                case B:
                    // positional scores
                    score += bishop_score[square];
                    material += material_score[BISHOP];
                    // mobility
                    //score += count_bits(get_bishop_attacks(square, occupancies[both]));
                    
                    break;
                
                // evaluate white rooks
                case R:
                    // positional score
                    score += rook_score[square];
                    material += material_score[ROOK];
                    // semi open file
                    /*if ((bitboards[P] & file_masks[square]) == 0)
                        // add semi open file bonus
                        score += semi_open_file_score;
                    
                    // semi open file
                    if (((bitboards[P] | bitboards[p]) & file_masks[square]) == 0)
                        // add semi open file bonus
                        score += open_file_score;*/
                    
                    break;
                
                // evaluate white queens
                case Q:
                    score += queen_score[square];
                    material += material_score[QUEEN];

                    // mobility
                    //score += count_bits(get_queen_attacks(square, occupancies[both]));
                    break;
                
                // evaluate white king
                case K:
                    // posirional score
                    score += king_score[square];
                    
                    // semi open file
                    // if ((bitboards[P] & file_masks[square]) == 0)
                    //     // add semi open file penalty
                    //     score -= semi_open_file_score;
                    
                    // // semi open file
                    // if (((bitboards[P] | bitboards[p]) & file_masks[square]) == 0)
                    //     // add semi open file penalty
                    //     score -= open_file_score;
                    
                    // // king safety bonus
                    // score += count_bits(king_attacks[square] & occupancies[white]) * king_shield_bonus;
                    break;

                // evaluate black pawns
                case p:
                    // positional score
                    score -= pawn_score[mirror_score[square]];
                    material -= material_score[PAWN];

                    // double pawn penalty
                    /*double_pawns = count_bits(bitboards[p] & file_masks[square]);
                    
                    // on double pawns (tripple, etc)
                    if (double_pawns > 1)
                        score -= double_pawns * double_pawn_penalty;
                    
                    // on isolated pawnd
                    if ((bitboards[p] & isolated_masks[square]) == 0)
                        // give an isolated pawn penalty
                        score -= isolated_pawn_penalty;
                    
                    // on passed pawn
                    if ((black_passed_masks[square] & bitboards[P]) == 0)
                        // give passed pawn bonus
                        score -= passed_pawn_bonus[get_rank[mirror_score[square]]];*/

                    break;
                
                // evaluate black knights
                case n:
                    // positional score
                    score -= knight_score[mirror_score[square]];
                    material -= material_score[KNIGHT];

                    break;
                
                // evaluate black bishops
                case b:
                    // positional score
                    score -= bishop_score[mirror_score[square]];
                    material -= material_score[BISHOP];

                    
                    // mobility
                    //score -= count_bits(get_bishop_attacks(square, occupancies[both]));
                    break;
                
                // evaluate black rooks
                case r:
                    // positional score
                    score -= rook_score[mirror_score[square]];
                    material -= material_score[ROOK];
                    
                    // semi open file
                    /*if ((bitboards[p] & file_masks[square]) == 0)
                        // add semi open file bonus
                        score -= semi_open_file_score;
                    
                    // semi open file
                    if (((bitboards[P] | bitboards[p]) & file_masks[square]) == 0)
                        // add semi open file bonus
                        score -= open_file_score;*/
                    
                    break;
                
                // evaluate black queens
                case q:
                    score -= queen_score[mirror_score[square]];
                    material -= material_score[QUEEN];

                    // mobility

                    //score -= count_bits(get_queen_attacks(square, occupancies[both]));
                    break;
                
                // evaluate black king
                case k:
                    // positional score
                    score -= king_score[mirror_score[square]];
                    
                    // semi open file
                    /*if ((bitboards[p] & file_masks[square]) == 0)
                        // add semi open file penalty
                        score += semi_open_file_score;
                    
                    // semi open file
                    if (((bitboards[P] | bitboards[p]) & file_masks[square]) == 0)
                        // add semi open file penalty
                        score += open_file_score;
                    
                    // king safety bonus
                    score -= count_bits(king_attacks[square] & occupancies[black]) * king_shield_bonus;*/
                    break;
            }

            // pop ls1b
            pop_bit(bitboard, square);
        }
    }
    
    // return final evaluation based on side
    return (side == white) ? material : -material;
}


/**********************************\
 ==================================
 
               Search
 
 ==================================
\**********************************/

// MVV LVA [attacker][victim]
static int mvv_lva[12][12] = {
 	105, 205, 305, 405, 505, 605,  105, 205, 305, 405, 505, 605,
	104, 204, 304, 404, 504, 604,  104, 204, 304, 404, 504, 604,
	103, 203, 303, 403, 503, 603,  103, 203, 303, 403, 503, 603,
	102, 202, 302, 402, 502, 602,  102, 202, 302, 402, 502, 602,
	101, 201, 301, 401, 501, 601,  101, 201, 301, 401, 501, 601,
	100, 200, 300, 400, 500, 600,  100, 200, 300, 400, 500, 600,

	105, 205, 305, 405, 505, 605,  105, 205, 305, 405, 505, 605,
	104, 204, 304, 404, 504, 604,  104, 204, 304, 404, 504, 604,
	103, 203, 303, 403, 503, 603,  103, 203, 303, 403, 503, 603,
	102, 202, 302, 402, 502, 602,  102, 202, 302, 402, 502, 602,
	101, 201, 301, 401, 501, 601,  101, 201, 301, 401, 501, 601,
	100, 200, 300, 400, 500, 600,  100, 200, 300, 400, 500, 600
};


// killer moves [count]
int killer_moves[2];

// history moves [side][from][to]
int history_moves[2][64][64];

/*
      ================================
            Triangular PV table
      --------------------------------
        PV line: e2e4 e7e5 g1f3 b8c6
      ================================

           0    1    2    3    4    5
      
      0    m1   m2   m3   m4   m5   m6
      
      1    0    m2   m3   m4   m5   m6 
      
      2    0    0    m3   m4   m5   m6
      
      3    0    0    0    m4   m5   m6
       
      4    0    0    0    0    m5   m6
      
      5    0    0    0    0    0    m6
*/

// PV length [ply]3
int pv_length[64];

// PV table [ply][ply]
int pv_table[64][64];

// follow PV & score PV move
int follow_pv, score_pv;


/**********************************\
 ==================================
 
        Transposition table
 
 ==================================
\**********************************/

// transposition table hash flags
#define FLAG_NONE 0
#define FLAG_EXACT 1
#define FLAG_ALPHA 2
#define FLAG_BETA 3

// transposition table data structure
typedef struct {
    U64 key;   // "almost" unique chess position identifier
    int depth;      // current search depth
    int flag;       // flag the type of node (fail-low/fail-high/PV) 
    int score;
    int move;      // score (alpha/beta/PV)
} ttEntry;               // transposition table (TT aka hash table)

#define HASH_SIZE (16 * 1024 * 1024)/sizeof(ttEntry)

typedef struct {
    ttEntry entries[HASH_SIZE];
}tt;

tt hash_table;

// clear TT (hash table)
void clear_hash_table()
{
    // loop over TT elements
    for (int index = 0; index < HASH_SIZE; index++)
    {
        // reset TT inner fields
        hash_table.entries[index].key = 0;
        hash_table.entries[index].depth = 0;
        hash_table.entries[index].flag = 0;
        hash_table.entries[index].score = 0;
        hash_table.entries[index].move = 0;
    }
}

void store_entry(U64 key, int f, int move, int depth, int bestScore){
    int n = HASH_SIZE;
    int i = reduce_hash(key, n);
    ttEntry entry;
    entry.score = bestScore;
    entry.flag = f;
    entry.depth = depth;
    entry.key = key;
    entry.move = move;
    hash_table.entries[i] = entry;
}

ttEntry probe_entry (U64 key){
    int n = HASH_SIZE;
    int i = reduce_hash(key, n);
    return hash_table.entries[i];
}

// enable PV move scoring
static inline void enable_pv_scoring(moves *move_list)
{
    // disable following PV
    follow_pv = 0;
    
    // loop over the moves within a move list
    for (int count = 0; count < move_list->count; count++)
    {
        // make sure we hit PV move
        if (pv_table[0][ply] == move_list->moves[count])
        {
            // enable move scoring
            score_pv = 1;
            
            // enable following PV
            follow_pv = 1;
        }
    }
}

/*  =======================
         Move ordering
    =======================
    
    1. PV move
    2. 1st killer move
    3. 2nd killer move
    4. Captures in MVV/LVA
    5. History moves
    6. Unsorted moves
*/

// score moves
static inline int score_move(int move, int tt_move)
{
    
    if (move == tt_move){
        return 100000;
    }
    // if PV move scoring is allowed
    if (score_pv)
    {
        
        if (pv_table[0][ply] == move)
        {
            score_pv = 0;
            return 20000;
        }
    }
    
    // score capture move
    if (get_move_capture(move))
    {
        // init target piece
        int target_piece = P;
        
        // pick up bitboard piece index ranges depending on side
        int start_piece, end_piece;
        
        // pick up side to move
        if (side == white) { start_piece = p; end_piece = k; }
        else { start_piece = P; end_piece = K; }
        
        // loop over bitboards opposite to the current side to move
        for (int bb_piece = start_piece; bb_piece <= end_piece; bb_piece++)
        {
            // if there's a piece on the target square
            if (get_bit(bitboards[bb_piece], get_move_target(move)))
            {
                // remove it from corresponding bitboard
                target_piece = bb_piece;
                break;
            }
        }
                
        // score move by MVV LVA lookup [source piece][target piece]
        return mvv_lva[get_move_piece(move)][target_piece] + 10000;
    }
    
    // score quiet move
    else
    {
        // score 1st killer move
        if (killer_moves[0] == move)
            return 9000;
        
        // score 2nd killer move
        else if (killer_moves[1] == move)
            return 8000;
        
        // score history move
        else
            return history_moves[side][get_move_source(move)][get_move_target(move)];
    }
    
    return 0;
}

// sort moves in descending order
static inline int sort_moves(moves *move_list, int tt_move)
{
    // move scores
    int move_scores[256];
    
    // score all the moves within a move list
    for (int count = 0; count < move_list->count; count++)
        // score move
        move_scores[count] = score_move(move_list->moves[count], tt_move);
    
    // loop over current move within a move list
    for (int current_move = 0; current_move < move_list->count; current_move++)
    {
        // loop over next move within a move list
        for (int next_move = current_move + 1; next_move < move_list->count; next_move++)
        {
            // compare current and next move scores
            if (move_scores[current_move] < move_scores[next_move])
            {
                // swap scores
                int temp_score = move_scores[current_move];
                move_scores[current_move] = move_scores[next_move];
                move_scores[next_move] = temp_score;
                
                // swap moves
                int temp_move = move_list->moves[current_move];
                move_list->moves[current_move] = move_list->moves[next_move];
                move_list->moves[next_move] = temp_move;
            }
        }
    }
}

// print move scores
void print_move_scores(moves *move_list)
{
    printf("     Move scores:\n\n");
        
    // loop over moves within a move list
    for (int count = 0; count < move_list->count; count++)
    {
        printf("     move: ");
        print_move(move_list->moves[count]);
        printf(" score: %d\n", score_move(move_list->moves[count], 0));
    }
}

// position repetition detection
static inline int is_repetition()
{
    int repetitions_found = 0;
    for (int index = 0; index < repetition_index; index++)
        
        if (repetition_table[index] == hash_key)
            repetitions_found++;
            if (repetitions_found >= 2){
                return 1;
            }
    
    // if no repetition found
    return 0;
}

// quiescence search
static inline int quiescence(int alpha, int beta)
{
    // every 2047 nodes
    if((nodes & 2047 ) == 0)
        // "listen" to the GUI/user input
		communicate();
	
    // increment nodes count
    nodes++;

    // we are too deep, hence there's an overflow of arrays relying on max ply constant
    if (ply > MAX_PLY - 1)
        // evaluate position
        return evaluate();

    // evaluate position
    int evaluation = evaluate();
    
    // fail-hard beta cutoff
    if (evaluation >= beta)
    {
        // node (position) fails high
        return beta;
    }
    
    // found a better move
    if (evaluation > alpha)
    {
        // PV node (position)
        alpha = evaluation;
    }
    
    // create move list instance
    moves move_list[1];
    
    // generate moves
    generate_moves(move_list);
    
    // sort moves
    sort_moves(move_list, 0);
    
    // loop over moves within a movelist
    for (int count = 0; count < move_list->count; count++)
    {
        // preserve board state
        copy_board();
        
        // increment ply
        ply++;
        
        // increment repetition index & store hash key
        repetition_index++;
        repetition_table[repetition_index] = hash_key;

        
        // make sure to make only legal moves
        if (make_move(move_list->moves[count], only_captures) == 0)
        {
            // decrement ply
            ply--;
            
            // decrement repetition index
            repetition_index--;
            
            // skip to next move
            continue;
        }

        // score current move
        int score = -quiescence(-beta, -alpha);
        
        // decrement ply
        ply--;
        
        // decrement repetition index
        repetition_index--;

        // take move back
        take_back();
        
        // reutrn 0 if time is up
        if(stopped == 1) return 0;
    
        if (score > evaluation){
            if (score > alpha)
            {
                alpha = score;
                if (score >= beta)
                {
                    return beta;
                }
            }
        }
    }
    
    // node (position) fails low
    return alpha;
}

const int full_depth_moves = 4;
const int reduction_limit = 3;

// negamax alpha beta search
static inline int negamax(int alpha, int beta, int depth, int is_root, int is_null)
{
    
    // Dont check for repetition if its in Root of the search.
    // If its a repetition, return 0
    //if (!is_root && is_repetition()){
    //    return 0;
    //}
    // every 2047 nodes
    if ((nodes & 2047) == 0){
        // "listen" to the GUI/user input
        communicate();
    }

    if (ply > MAX_PLY - 1)
    {
        // evaluate position
        return evaluate();
    }
    
    // init PV length
    pv_length[ply] = ply;

    // recursion escapre condition
    if (depth == 0)
    {
        // run quiescence search
        return quiescence(alpha, beta);
        //return evaluate();
    }
    
    int pv_node = beta - alpha > 1;
    int oldAlpha = alpha;
    int score;
    int posEval = evaluate();

    // increment nodes count
    nodes++;

    // is king in check
    int in_check = is_square_attacked((side == white) ? get_ls1b_index(bitboards[K]) : get_ls1b_index(bitboards[k]),
                                      side ^ 1);

    // increase search depth if the king has been exposed into a check
    if (in_check){
        depth++;
    }

    // legal moves counter
    int legal_moves = 0;
    

    // number of moves searched in a move list
    int moves_searched = 0;
    int best = -999999;

    ttEntry tte = probe_entry(hash_key);

    if ((ply != 0) && (hash_key == tte.key) && (tte.depth >= depth))
    {
        if (tte.flag == FLAG_EXACT)
        {
            return tte.score;
        }
        else if (tte.flag == FLAG_BETA)
        {
            alpha = MAX(alpha, tte.score);
        }
        else if (tte.flag == FLAG_ALPHA)
        {
            beta = MIN(beta, tte.score);
        }
        if (alpha >= beta)
        {
            return tte.score;
        }
    }

    // create move list instance
    moves move_list[1];

    // generate moves
    generate_moves(move_list);
    // if we are now following PV line
    if (follow_pv)
    {
        // enable PV move scoring
        enable_pv_scoring(move_list);
    }
    // sort moves
    sort_moves(move_list, tte.move);
    // loop over moves within a movelist

    for (int count = 0; count < move_list->count; count++)
    {
        // preserve board state
        copy_board();

        // increment ply
        ply++;

        // increment repetition index & store hash key
        repetition_index++;
        repetition_table[repetition_index] = hash_key;

        // make sure to make only legal moves
        if (make_move(move_list->moves[count], all_moves) == 0)
        {
            // decrement ply
            ply--;

            // decrement repetition index
            repetition_index--;

            // skip to next move
            continue;
        }

        // increment legal moves
        legal_moves++;

        // search with full depth but reduced window
        if (!pv_node || moves_searched > 0){
            score = -negamax(-alpha-1, -alpha, depth - 1, 0, is_null);
        }

        // PVS search.
        if (pv_node && (moves_searched == 0 || (score > alpha && score < beta))){
            score = -negamax(-beta, -alpha, depth - 1, 0, is_null);
        }
        
        // decrement ply
        ply--;

        // decrement repetition index
        repetition_index--;

        // take move back
        take_back();

        // reutrn 0 if time is up
        if (!is_root && stopped == 1) // Dont return 0 if its root
            return 0;

        // increment the counter of moves searched so far
        moves_searched++;
        
        // found a better move
        if (score > best){
            best = score;
            
            pv_table[ply][ply] = move_list->moves[count];

            for (int next_ply = ply + 1; next_ply < pv_length[ply + 1]; next_ply++){
                 pv_table[ply][next_ply] = pv_table[ply + 1][next_ply];
            }
            
            pv_length[ply] = pv_length[ply + 1];
            
            if (score > alpha)
            {
                alpha = score;
                if (score >= beta)
                {
                    // on quiet moves
                    if (get_move_capture(move_list->moves[count]) == 0)
                    {
                            // store killer moves
                            killer_moves[1] = killer_moves[0];
                            killer_moves[0] = move_list->moves[count];

                            //store history
                            history_moves[side][get_move_source(move_list->moves[count])][get_move_target(move_list->moves[count])] += depth*depth;
                    }
                    break;
                }
            }
        }
        history_moves[side][get_move_source(move_list->moves[count])][get_move_target(move_list->moves[count])] -= depth*depth;
        
        if (is_root && stopped == 1){
            break;
        } // if time is up and we are in root, break.
    }

    if (legal_moves == 0)
    {
        if (in_check)
        {
            return -MATE_VALUE + ply;
        }
        
        else
        {
            return 0;
        }
    }

    int bound = 0;

    if (best <= oldAlpha)
    {
        bound = FLAG_ALPHA;
    }
    else if (best >= beta)
    {
        bound = FLAG_BETA;
    }
    else
    {
        bound = FLAG_EXACT;
    }

    store_entry(hash_key, bound, pv_table[0][0], depth, best);
    
    return alpha;
}

// search position for the best move
void search_position(int depth)
{
    // define best score variable
    int score = 0;
    
    // reset nodes counter
    nodes = 0;
    
    // reset "time is up" flag
    stopped = 0;
    
    // reset follow PV flags
    follow_pv = 0;
    score_pv = 0;
    
    // clear helper data structures for search
    
    memset(pv_table, 0, sizeof(pv_table));
    memset(pv_length, 0, sizeof(pv_length));

    memset(history_moves, 0, sizeof(history_moves));
    // define initial alpha beta bounds
    int alpha = -INFINITE;
    int beta = INFINITE;
 
    int best_move = 0;
    
    // iterative deepening
    for (int current_depth = 1; current_depth <= depth; current_depth++)
    {
        
        if(stopped == 1){
			// stop calculating and return best move so far 
			break;
        }
        // enable follow PV flag
        follow_pv = 1;
        
        killer_moves[0] = 0;
        killer_moves[1] = 0;


        // find best move within a given position
        score = negamax(alpha, beta, current_depth, 1, 1);
        
        best_move = pv_table[0][0];
        if (score > -MATE_VALUE && score < -MATE_SCORE){
            printf("info score mate %d depth %d nodes %ld time %d pv ", -(score + MATE_VALUE) / 2 - 1, current_depth, nodes, get_time_ms() - starttime);
        }
        else if (score > MATE_SCORE && score < MATE_VALUE){
            printf("info score mate %d depth %d nodes %ld time %d pv ", (MATE_VALUE - score) / 2 + 1, current_depth, nodes, get_time_ms() - starttime);   
        
        }else{
            printf("info score cp %d depth %d nodes %ld time %d pv ", score, current_depth, nodes, get_time_ms() - starttime);
        }
        // loop over the moves within a PV line
        for (int count = 0; count < pv_length[0]; count++)
        {
            // print PV move
            print_move(pv_table[0][count]);
            printf(" ");
        }
        
        // print new line
        printf("\n");
    }

    
    
    if (best_move == 0){
        best_move = pv_table[0][0];
    }
    printf("bestmove ");
    print_move(best_move);
    printf("\n");
}

/**********************************\
 ==================================
 
                UCI
          forked from VICE
         by Richard Allbert
 
 ==================================
\**********************************/

// parse user/GUI move string input (e.g. "e7e8q")
int parse_move(char *move_string)
{
    // create move list instance
    moves move_list[1];
    
    // generate moves
    generate_moves(move_list);
    
    // parse source square
    int source_square = (move_string[0] - 'a') + (8 - (move_string[1] - '0')) * 8;
    
    // parse target square
    int target_square = (move_string[2] - 'a') + (8 - (move_string[3] - '0')) * 8;
    
    // loop over the moves within a move list
    for (int move_count = 0; move_count < move_list->count; move_count++)
    {
        // init move
        int move = move_list->moves[move_count];
        
        // make sure source & target squares are available within the generated move
        if (source_square == get_move_source(move) && target_square == get_move_target(move))
        {
            // init promoted piece
            int promoted_piece = get_move_promoted(move);
            
            // promoted piece is available
            if (promoted_piece)
            {
                // promoted to queen
                if ((promoted_piece == Q || promoted_piece == q) && move_string[4] == 'q')
                    // return legal move
                    return move;
                
                // promoted to rook
                else if ((promoted_piece == R || promoted_piece == r) && move_string[4] == 'r')
                    // return legal move
                    return move;
                
                // promoted to bishop
                else if ((promoted_piece == B || promoted_piece == b) && move_string[4] == 'b')
                    // return legal move
                    return move;
                
                // promoted to knight
                else if ((promoted_piece == N || promoted_piece == n) && move_string[4] == 'n')
                    // return legal move
                    return move;
                
                // continue the loop on possible wrong promotions (e.g. "e7e8f")
                continue;
            }
            
            // return legal move
            return move;
        }
    }
    
    // return illegal move
    return 0;
}

/*
    Example UCI commands to init position on chess board
    
    // init start position
    position startpos
    
    // init start position and make the moves on chess board
    position startpos moves e2e4 e7e5
    
    // init position from FEN string
    position fen r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1 
    
    // init position from fen string and make moves on chess board
    position fen r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1 moves e2a6 e8g8
*/

// parse UCI "position" command
void parse_position(char *command)
{
    // shift pointer to the right where next token begins
    command += 9;
    
    // init pointer to the current character in the command string
    char *current_char = command;
    
    // parse UCI "startpos" command
    if (strncmp(command, "startpos", 8) == 0)
        // init chess board with start position
        parse_fen(start_position);
    
    // parse UCI "fen" command 
    else
    {
        // make sure "fen" command is available within command string
        current_char = strstr(command, "fen");
        
        // if no "fen" command is available within command string
        if (current_char == NULL)
            // init chess board with start position
            parse_fen(start_position);
            
        // found "fen" substring
        else
        {
            // shift pointer to the right where next token begins
            current_char += 4;
            
            // init chess board with position from FEN string
            parse_fen(current_char);
        }
    }
    
    // parse moves after position
    current_char = strstr(command, "moves");
    
    // moves available
    if (current_char != NULL)
    {
        // shift pointer to the right where next token begins
        current_char += 6;
        
        // loop over moves within a move string
        while(*current_char)
        {
            // parse next move
            int move = parse_move(current_char);
            
            // if no more moves
            if (move == 0)
                // break out of the loop
                break;
            
            // increment repetition index
            repetition_index++;
            
            // wtire hash key into a repetition table
            repetition_table[repetition_index] = hash_key;
            
            // make move on the chess board
            make_move(move, all_moves);
            
            // move current character mointer to the end of current move
            while (*current_char && *current_char != ' ') current_char++;
            
            // go to the next move
            current_char++;
        }        
    }
}

// parse UCI command "go"
void parse_go(char *command)
{
    // init parameters
    int depth = -1;

    // init argument
    char *argument = NULL;

    // infinite search
    if ((argument = strstr(command,"infinite"))) {}

    // match UCI "binc" command
    if ((argument = strstr(command,"binc")) && side == black)
        // parse black time increment
        inc = atoi(argument + 5);

    // match UCI "winc" command
    if ((argument = strstr(command,"winc")) && side == white)
        // parse white time increment
        inc = atoi(argument + 5);

    // match UCI "wtime" command
    if ((argument = strstr(command,"wtime")) && side == white)
        // parse white time limit
        uci_time = atoi(argument + 6);

    // match UCI "btime" command
    if ((argument = strstr(command,"btime")) && side == black)
        // parse black time limit
        uci_time = atoi(argument + 6);

    // match UCI "movestogo" command
    if ((argument = strstr(command,"movestogo")))
        // parse number of moves to go
        movestogo = atoi(argument + 10);

    // match UCI "movetime" command
    if ((argument = strstr(command,"movetime")))
        // parse amount of time allowed to spend to make a move
        uci_time = atoi(argument + 9);

    // match UCI "depth" command
    if ((argument = strstr(command,"depth")))
        // parse search depth
        depth = atoi(argument + 6);

    // if move time is not available
    if(movetime != -1)
    {
        // set time equal to move time
        uci_time = movetime;

        // set moves to go to 1
        movestogo = 1;
    }

    // init start time
    starttime = get_time_ms();

    // init search depth
    depth = depth;

    // if time control is available
    if(uci_time != -1)
    {
        // flag we're playing with time control
        timeset = 1;

        // set up timing
        uci_time /= movestogo;
        if (uci_time > 1500) {
            uci_time -= 50;
        }
        stoptime = starttime + uci_time + inc;
    }

    // if depth is not available
    if(depth == -1)
        // set depth to 64 plies (takes ages to complete...)
        depth = 64;

    // print debug info
    printf("time:%d start:%d stop:%d depth:%d timeset:%d\n",
    uci_time, starttime, stoptime, depth, timeset);

    // search position
    search_position(depth);
}

// main UCI loop
void uci_loop()
{
    // reset STDIN & STDOUT buffers
    setbuf(stdin, NULL);
    setbuf(stdout, NULL);
    
    // define user / GUI input buffer
    char input[2000];
    
    // print engine info
    printf("id name %s v%s\n", _ENGINE_NAME, _ENGINE_VERSION);
    printf("id name %s\n", _ENGINE_AUTHOR);
    printf("uciok\n");
    
    // main loop
    while (1)
    {
        // reset user /GUI input
        memset(input, 0, sizeof(input));
        
        // make sure output reaches the GUI
        fflush(stdout);
        
        // get user / GUI input
        if (!fgets(input, 2000, stdin))
            // continue the loop
            continue;
        
        // make sure input is available
        if (input[0] == '\n')
            // continue the loop
            continue;
        
        // parse UCI "isready" command
        if (strncmp(input, "isready", 7) == 0)
        {
            printf("readyok\n");
            continue;
        }
        
        // parse UCI "position" command
        else if (strncmp(input, "position", 8) == 0)
        {
            // call parse position function
            parse_position(input);
        
            // clear hash table
            clear_hash_table();
        }
        // parse UCI "ucinewgame" command
        else if (strncmp(input, "ucinewgame", 10) == 0)
        {
            // call parse position function
            parse_position("position startpos");
            
            // clear hash table
            clear_hash_table();
        }
        // parse UCI "go" command
        else if (strncmp(input, "go", 2) == 0)
            // call parse go function
            parse_go(input);
        
        // parse UCI "quit" command
        else if (strncmp(input, "quit", 4) == 0)
            // quit from the chess engine program execution
            break;
        
        // parse UCI "uci" command
        else if (strncmp(input, "uci", 3) == 0)
        {
            // print engine info
            printf("id name %s v%s\n", _ENGINE_NAME, _ENGINE_VERSION);
            printf("id name %s\n", _ENGINE_AUTHOR);
            printf("uciok\n");
        }
        else if (strncmp(input, "eval", 4) == 0)
        {
            printf("eval: %d\n", evaluate());
        }
        else if (strncmp(input, "checkstop", 8) == 0)
        {
            printf("stopped: %d\n", stopped);
        }else if (strncmp(input, "print", 5) == 0)
        {
            print_board();
        }
    }
}


/**********************************\
 ==================================
 
              Init all
 
 ==================================
\**********************************/

// init all variables
void init_all()
{
    // init leaper pieces attacks
    init_leapers_attacks();
    
    // init slider pieces attacks
    init_sliders_attacks(bishop);
    init_sliders_attacks(rook);
    
    // init magic numbers
    //init_magic_numbers();
    
    // init random keys for hashing purposes
    init_random_keys();
    init_evaluation_masks();
    init_tables();

    // clear hash table
    clear_hash_table();

    for (int depth = 1; depth < 64; depth++)
        for (int played = 1; played < 64; played++)
            LMRTable[depth][played] = 0.75 + log(depth) * log(played) / 2.25;
}


/**********************************\
 ==================================
 
             Main driver
 
 ==================================
\**********************************/

int main()
{
    // init all
    init_all();

    // debug mode variable
    int debug = 0;
    
    // if debugging
    if (debug)
    {
       parse_fen("r1bqkb1r/pppp1ppp/2n5/4p1B1/3Pn3/2N2N2/PPP2PPP/R2QKB1R w KQkq - 1 5");
       moves moveslist[1];
       generate_moves(moveslist);
       search_position(5);
       sort_moves(moveslist, 0);
       print_move_scores(moveslist);
    }
    
    else
        // connect to the GUI
        uci_loop();

    return 0;
}
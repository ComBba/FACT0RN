// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pow.h>

#include <arith_uint256.h>
#include <chain.h>
#include <primitives/block.h>
#include <uint256.h>
#include <algorithm>  //min and max.
#include <iostream>
#include <cassert>
#include <cmath>

//Blake2b, Scrypt and SHA3-512
#include "cryptopp/cryptlib.h"
#include "cryptopp/sha3.h"
#include "cryptopp/whrlpool.h"
#include "cryptopp/scrypt.h"
#include "cryptopp/secblock.h"
#include "cryptopp/blake2.h"
#include "cryptopp/hex.h"
#include "cryptopp/files.h"

//Fancy popcount implementation
#include "libpopcnt.h"

uint16_t GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    assert(pindexLast != nullptr);

    // Only change once per difficulty adjustment interval
    if ((pindexLast->nHeight+1) % params.DifficultyAdjustmentInterval() != 0)
    {
        if (params.fPowAllowMinDifficultyBlocks)
        {
            // Special difficulty rule for testnet:
            // If the new block's timestamp is more than 2* 10 minutes
            // then allow mining of a min-difficulty block.
            if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing*2){
                return params.powLimit;
            }
            else
            {
                // Return the last non-special-min-difficulty-rules-block
                const CBlockIndex* pindex = pindexLast;
                while (pindex->pprev && pindex->nHeight % params.DifficultyAdjustmentInterval() != 0 &&
                       pindex->nBits == params.powLimit)
                {
                    pindex = pindex->pprev;
                }
                return pindex->nBits;
            }
        }
        return pindexLast->nBits;
    }

    // Go back by what we want to be 14 days worth of blocks
    int nHeightFirst = pindexLast->nHeight - (params.DifficultyAdjustmentInterval()-1);
    assert(nHeightFirst >= 0);
    const CBlockIndex* pindexFirst = pindexLast->GetAncestor(nHeightFirst);
    assert(pindexFirst);
    
    return CalculateNextWorkRequired(pindexLast, pindexFirst->GetBlockTime(), params);
}

uint16_t CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // Compute constants
    const int64_t               nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    const double  nPeriodTimeProportionConsumed = (double)nActualTimespan/(double)params.nPowTargetTimespan;    

    //Handle extremes
    if( nPeriodTimeProportionConsumed >= 1.40f)
        return (int32_t)pindexLast->nBits - 4;
    if( nPeriodTimeProportionConsumed <= 0.60)
        return (int32_t)pindexLast->nBits + 4;

    //Compute Retargeting function
    const int32_t condition = nPeriodTimeProportionConsumed > 1; 
    const int32_t maxAdjust = ( condition ) ? -4: 4;
    const double         Fx = (1.0f)/(2*nPeriodTimeProportionConsumed - 1 - 2*condition) - 1 + 2*condition; 
    const int32_t   roundFx = round( Fx );
    const int32_t nRetarget = condition ? std::max( maxAdjust, roundFx ) : std::min( maxAdjust, roundFx ) ;

    return (int32_t)pindexLast->nBits + nRetarget; 
}

bool CheckProofOfWork( const CBlockHeader& block, const Consensus::Params& params)
{

    //First, generate the random seed submited for this block
    uint1024 w = gHash( block, params );

    //Check that |block->offset| <= \tilde{n} = 16 * |n|_2.
    uint64_t abs_offset = ( block.wOffset > 0) ? block.wOffset: - block.wOffset;

    if (  abs_offset > 16 * block.nBits ){
        std::cout << "wOffset value does not validate." <<std::endl;
        return false;
    }

    //Get the semiprime n
    mpz_t n, W;
    mpz_init(n);
    mpz_init(W);
    mpz_import( W, 16, -1, 8, 0, 0, w.u64_begin());

    //Add the offset to w to find the semiprime submitted: n = w + offset
    if( block.wOffset >= 0){
    	mpz_add_ui(n, W, abs_offset); 
    } else {
    	mpz_sub_ui(n, W, abs_offset); 
    }

    gmp_printf("  W: %Zd \n", W);
    gmp_printf("  N: %Zd \n", n);

    //Clear memory for W.
    mpz_clear(W);

    //Check the number n has nBits
    if( mpz_sizeinbase(n, 2) != block.nBits){
       std::cout << "nBits value does not validate. Expected nBits = "<< block.nBits <<", actual value is " << mpz_sizeinbase(n, 2) << ". "  <<std::endl;
       mpz_clear(n);
       return false; 
    }
  
    //Divide the factor submitted for this block by N
    mpz_t nP1, nP2;
    mpz_init( nP1 );
    mpz_init( nP2 );
    mpz_import( nP1, 16 , -1, 8, 0, 0, block.nP1.u64_begin() );
    mpz_tdiv_q(nP2, n, nP1);
    
    gmp_printf("nP1: %Zd \n", nP1);
    gmp_printf("nP2: %Zd \n", nP2);
    
    //Check nP1 is a factor
    mpz_t n_check;
    mpz_init( n_check );
    mpz_mul( n_check, nP1, nP2);

    //Check that nP1*nP2 == n and that nP1 <= nP2.
    if( (mpz_cmp(n_check, n) != 0) || (mpz_cmp(nP1, nP2) > 0)   )
    {
        std::cout << "nP1 value does not validate." <<std::endl;
        gmp_printf("N   = %Zd \n", n);
        gmp_printf("nP1 = %Zd \n", nP1);
        std::cout << "nP1 does not divide N or nP1 >= nP2. Likely, the former." << std::endl;
        mpz_clear(n);
        mpz_clear(nP1);
        mpz_clear(nP2);
        mpz_clear(n_check);
    	return false;
    }

    //Clear memory
    mpz_clear(n);
    mpz_clear(n_check);

    //Test nP1 and nP2 for primality.
    int is_nP1_prime = mpz_probab_prime_p( nP1,  params.MillerRabinRounds);
    int is_nP2_prime = mpz_probab_prime_p( nP2,  params.MillerRabinRounds);

    //Clear memory
    mpz_clear(nP1);
    mpz_clear(nP2);

    //Check they are both prime
    if ( is_nP1_prime == 0 || is_nP2_prime == 0){
        std::cout << "At least 1 composite factor found. Reject submission." <<std::endl;
	return false;
    }

    return true;
}

uint1024 gHash( const CBlockHeader& block, const Consensus::Params& params)
{
    //Get the required data for this block
    uint256  hashPrevBlock       = block.hashPrevBlock;
    uint256  hashMerkleRoot      = block.hashMerkleRoot;
    uint64_t nNonce              = block.nNonce;
    uint32_t nTime               = block.nTime;
    int32_t  nVersion            = block.nVersion;
    uint16_t nBits               = block.nBits;
   
    using namespace CryptoPP;
   
    //Place data as raw bytes into the password and salt for Scrypt:
    /////////////////////////////////////////////////
    // pass = hashPrevBlock + hashMerkle + nNonce  //
    // salt = version       + nBits      + nTime   //
    /////////////////////////////////////////////////
    byte pass[ 256/8 + 256/8 + 64/8 ] = { (byte) 0 };
    byte salt[  32/8 +  16/8 + 32/8 ] = { (byte) 0 };

    //SALT: Copy version into the first 4 bytes of the salt.
    memcpy( salt, &nVersion, sizeof(nVersion) );
    
    //SALT: Copy nBits into the next 2 bytes
    int runningLen = sizeof(nVersion);
    memcpy( &salt[runningLen], &nBits, sizeof(nBits) );

    //SALT: Copy nTime into the next 4 bytes
    runningLen += sizeof(nBits);
    memcpy( &salt[runningLen], &nTime, sizeof(nTime) );

    //PASS: Copy Previous Block Hash into the first 32 bytes
    memcpy( pass, hashPrevBlock.begin(), hashPrevBlock.size() );

    //PASS: Copy Merkle Root hash into next 32 bytes
    runningLen = hashPrevBlock.size();
    memcpy( &pass[runningLen], hashMerkleRoot.begin(), hashMerkleRoot.size() );

    //PASS: Copy nNonce
    runningLen += hashMerkleRoot.size();
    memcpy( &pass[runningLen], &nNonce, sizeof(nNonce) );

    ////////////////////////////////////////////////////////////////////////////////
    //                                Scrypt parameters                           //
    ////////////////////////////////////////////////////////////////////////////////
    //                                                                            //
    //  N                  = Iterations count (Affects memory and CPU Usage).     //
    //  r                  = block size ( affects memory and CPU usage).          //
    //  p                  = Parallelism factor. (Number of threads).             //
    //  pass               = Input password.                                      //
    //  salt               = securely-generated random bytes.                     //
    //  derived-key-length = how many bytes to generate as output. Defaults to 32.//
    //                                                                            //
    // For reference, Litecoin has N=1024, r=1, p=1.                              //
    ////////////////////////////////////////////////////////////////////////////////
    Scrypt scrypt;
    word64 N = 1ULL << 12;
    word64 r = 1ULL << 1;
    word64 p = 1ULL; 
    SecByteBlock derived(256);

    //Scrypt Hash to 2048-bits hash. 
    scrypt.DeriveKey(derived, derived.size(), pass, sizeof(pass), salt, sizeof(salt), N, r, p);

    //Consensus parameters
    int roundsTotal = params.hashRounds;

    //Additional hashes we will need
    SHA3_512 sHash;

    //Prepare GMP objects
    mpz_t prime_mpz, starting_number_mpz, a_mpz, a_inverse_mpz;
    mpz_init(prime_mpz);
    mpz_init(starting_number_mpz);
    mpz_init(a_mpz);
    mpz_init(a_inverse_mpz);

    for(int round =0; round < roundsTotal; round++ ){

        ///////////////////////////////////////////////////////////////
	//      Memory Expensive Scrypt: 1MB required.              //
	///////////////////////////////////////////////////////////////
	scrypt.DeriveKey(        derived,  //Final hash
                      derived.size(),  //Final hash number of bytes
         (const byte*)derived.data(),  //Input hash
                      derived.size(),  //Input hash number of bytes 
                                salt,  //Salt
                        sizeof(salt),  //Salt bytes
                                   N,  //Number of rounds
                                   r,  //Sequential Read Sisze
                                   p   //Parallelizable iterations
        );

	///////////////////////////////////////////////////////////////
	//   Add different types of hashes to the core.              //
	///////////////////////////////////////////////////////////////
        //Count the bits in previous hash.
        uint64_t pcnt_half1 = popcnt(      derived.data(), 128 );
        uint64_t pcnt_half2 = popcnt( &derived.data()[128], 128 );

        //Hash the first 1024-bits of the 2048-bits hash.
        if( pcnt_half1 % 2 == 0 ){
            BLAKE2b bHash;
            bHash.Update((const byte*)derived.data(), 128 );
            bHash.Final((byte*)derived.data());
	}else{
            SHA3_512 bHash;
            bHash.Update((const byte*)derived.data(), 128 );
       	    bHash.Final((byte*)derived.data());
	}

        //Hash the second 1024-bits of the 2048-bits hash.
        if( pcnt_half2 % 2 == 0 ){
            BLAKE2b bHash;
            bHash.Update((const byte*)(&derived.data()[128]), 128 );
            bHash.Final((byte*)(&derived.data()[128]) );
        } else {
	    SHA3_512 bHash;
       	    bHash.Update((const byte*)(&derived.data()[128]), 128 );
            bHash.Final((byte*)(&derived.data()[128]) );
	}

	//////////////////////////////////////////////////////////////
	// Perform expensive math opertions plus simple hashing     //
	//////////////////////////////////////////////////////////////
        //Use the current hash to compute grunt work.
        mpz_import( starting_number_mpz, 32, -1, 8, 0, 0, derived.data() ); // -> M = 2048-hash
        mpz_sqrt( starting_number_mpz, starting_number_mpz);                // - \ a = floor( M^(1/2) )
        mpz_set(                a_mpz, starting_number_mpz);                // - /
        mpz_sqrt( starting_number_mpz, starting_number_mpz);                // - \ p = floor( a^(1/2) )
        mpz_nextprime(      prime_mpz, starting_number_mpz);                // - /

	//Compute a^(-1) Mod p
	mpz_invert( a_inverse_mpz, a_mpz, prime_mpz);

	//Xor into current hash digest.
	size_t words = 0;
        uint64_t data[32] = {0};
        uint64_t *hDigest = (uint64_t *) derived.data();	
        mpz_export( data, &words , -1, 8, 0, 0, a_inverse_mpz );
	for(int jj=0; jj < 32; jj++)  hDigest[jj] ^= data[jj]; 

        //Check that at most 2048-bits were written
	//Assume 64-bit limbs.
	assert( words <= 32);

        //Compute the population count of a_inverse
	const int32_t irounds   = popcnt( data , sizeof(data)  ) & 0x7f;
        
	//Branch away
	for( int jj=0; jj < irounds; jj++){    	
            const int32_t br = popcnt( derived.data(), sizeof(derived.data())  );

	    //Power mod
	    mpz_powm_ui(a_inverse_mpz, a_inverse_mpz, irounds, prime_mpz  );

            //Get the data out of gmp
            mpz_export( data, &words , -1, 8, 0, 0, a_inverse_mpz );
	    assert( words <= 32 );

	    for(int jj=0; jj < 32; jj++)  hDigest[jj] ^= data[jj]; 

            if( br % 3 == 0 )
	    {
                SHA3_512 bHash;
                bHash.Update((const byte*)derived.data(), 128 );
                bHash.Final(       (byte*)derived.data()      );
	    } else if ( br % 3 == 2)
	    {
                BLAKE2b sHash;
                sHash.Update((const byte*)(&derived.data()[128]), 128 );
                sHash.Final(       (byte*)(&derived.data()[192])      );
	    } else {
                Whirlpool wHash;
	        wHash.Update((const byte*)(derived.data()), 256);
	        wHash.Final((byte*)(&derived.data()[112])  );
	    }
	}

    }
    
    //Compute how many bytes to copy
    int32_t allBytes = nBits/8;
    int32_t remBytes = nBits % 8;

    //Make sure to stay within 2048-bits.
    // NOTE: In the distant future this will have to be updated
    //       when nBITS starts to get close to 1024-bits, the limit of type uint1024.
    assert( allBytes + 1 <= 128);

    //TODO: Note that we use here a type to that holds 1024-bits instead of the 
    //      2048 bits of the hash above. For now this is fine, eventually, we will
    //      need to implement a 2048-bit when the system is factoring 900+ digit numbers.
    //      As this is unlikely to happen any time soon I think we are fine.
    //Copy exactly the number of bytes that contains exactly the low nBits bits.
    uint1024 w;

    //Make sure the values in w are set to 0.
    memset(w.u8_begin_write(), 0, 128);

    memcpy( w.u8_begin_write(), derived.begin(), std::min( 128, allBytes + 1) );
    
    //Trim off any bits from the Most Significant byte.
    w.u8_begin_write()[allBytes] = w.u8_begin()[allBytes] & ( (1 << remBytes) - 1 );

    //Set the nBits-bit to one.
    if( remBytes == 0){
        w.u8_begin_write()[allBytes-1] = w.u8_begin()[allBytes - 1] | 128;
    } else {
        w.u8_begin_write()[allBytes] = w.u8_begin()[allBytes] | ( 1 << (remBytes-1) );
    }

    mpz_clear(prime_mpz);
    mpz_clear(starting_number_mpz);
    mpz_clear(a_mpz);
    mpz_clear(a_inverse_mpz);

    return w;
}

// f(z) = z^2 + 1 Mod n
void f( mpz_t z, mpz_t n, mpz_t two) {
    mpz_powm(z, z, two, n);
    mpz_add_ui(z, z, 1ULL );
    mpz_mod(z, z, n);
}

//  Pollard rho factoring algorithm
//  Input: g First found factor goes here
//         n Number to be factored.
// Output: Return  0 if n is prime.
//         Returns 1 if both factors g and n/g terminate being prime.
//         Return  0 otherwise.
int rho(mpz_t g, mpz_t n)
{
    //Parameter of 25 gives 1 in 2^100 chance of false positive. That's good enough.
    if(mpz_probab_prime_p(n,25) != 0)
        return 0;//test if n is prime with miller rabin test
    
    mpz_t x;
    mpz_t y;
    mpz_t two;
    mpz_t temp;
    mpz_init_set_ui(x ,2);
    mpz_init_set_ui(y ,2);//initialize x and y as 2
    mpz_init_set_ui(two ,2);
    mpz_set_ui(g , 1); 
    mpz_init(temp);

    while(mpz_cmp_ui(g,1) == 0)
    {   
        f(x,n, two);                //x is changed
        f(y,n, two); f(y,n, two);   //y is going through the sequence twice as fast

        mpz_sub(temp,x,y);
        mpz_gcd(g , temp, n); 
    }   

    //Set temp = n/g
    mpz_divexact(temp, n, g); 

    //Test if g, temp are prime.
    int u_p = mpz_probab_prime_p(temp,30);
    int g_p = mpz_probab_prime_p(g,30);

    //Clear gmp memory allocated inside this function
    mpz_clear(x);
    mpz_clear(y);
    mpz_clear(temp);
    mpz_clear(two);

    //Enforce primality checks
    //Check g is a proper factor. Pollard Rho may have missed a factorization.
    if( (u_p != 0) && (g_p != 0) && (mpz_cmp(g,n) != 0) ) 
        return 1;

    return 0;
}

int rho(uint64_t &g, uint64_t n)
{
    //Declare needed types
    mpz_t g1;
    mpz_t n1;

    //Initialize gmp types
    mpz_init(g1);
    mpz_init(n1);

    //Set gmp types
    mpz_set_ui( g1, g );
    mpz_set_ui( n1, n );

    //Factor and capture return value
    int result = rho(g1, n1);
    g = mpz_get_ui(g1);

    //Clear memory allocated inside this function
    mpz_clear(g1);
    mpz_clear(n1);

    //return 
    return result;

}



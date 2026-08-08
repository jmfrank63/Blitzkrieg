#ifndef __PLATFORM_LEGACYALGORITHM_H__
#define __PLATFORM_LEGACYALGORITHM_H__

#include <algorithm>
#include <cstdlib>
#include <iterator>

// std::random_shuffle was deprecated in C++14 and removed in C++17. The MSVC
// and libstdc++ standard libraries still ship it under their compatibility
// settings, but libc++ (macOS) enforces the removal.
#if !defined(_LIBCPP_VERSION)
#define BK_HAS_STD_RANDOM_SHUFFLE 1
#endif

namespace NPlatform
{

// Shuffle a range the way the engine always has. Where std::random_shuffle
// still exists it is called unchanged, so the Windows and Linux builds keep
// their exact historical sequence for map generation, multiplayer player
// ordering, and sound selection. The fallback reproduces the classic
// implementation: swap each element with a uniformly chosen position at or
// before it, drawn from std::rand(), so it stays tied to the same global
// seed the engine already sets with std::srand().
template <class RandomIt>
inline void RandomShuffle( RandomIt first, RandomIt last )
{
#if defined(BK_HAS_STD_RANDOM_SHUFFLE)
	std::random_shuffle( first, last );
#else
	typedef typename std::iterator_traits<RandomIt>::difference_type difference_type;
	if ( first == last ) return;
	for ( RandomIt i = first + 1; i != last; ++i )
	{
		const difference_type span = ( i - first ) + 1;
		std::iter_swap( i, first + static_cast<difference_type>( std::rand() % span ) );
	}
#endif
}

// Generator-driven overload. The engine passes its own deterministic Random(N)
// functor here, which AI behaviour and multiplayer synchronization depend on,
// so the fallback keeps the classic swap order and calls the generator once per
// element with the same argument std::random_shuffle used.
template <class RandomIt, class RandomNumberGenerator>
inline void RandomShuffle( RandomIt first, RandomIt last, RandomNumberGenerator &generator )
{
#if defined(BK_HAS_STD_RANDOM_SHUFFLE)
	std::random_shuffle( first, last, generator );
#else
	typedef typename std::iterator_traits<RandomIt>::difference_type difference_type;
	if ( first == last ) return;
	for ( RandomIt i = first + 1; i != last; ++i )
	{
		const difference_type span = ( i - first ) + 1;
		std::iter_swap( i, first + static_cast<difference_type>( generator( span ) ) );
	}
#endif
}

}

#endif // __PLATFORM_LEGACYALGORITHM_H__

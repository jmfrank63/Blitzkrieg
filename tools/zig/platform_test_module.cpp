#if defined(_WIN32) || defined(_WIN64) || defined(WIN32)
#define BK_EXPORT __declspec(dllexport)
#else
#define BK_EXPORT __attribute__((visibility("default")))
#endif

extern "C" BK_EXPORT int bk_platform_test_value()
{
	return 42;
}

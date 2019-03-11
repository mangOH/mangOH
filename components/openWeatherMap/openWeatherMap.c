#include "legato.h"
#include <curl/curl.h>
#include "openWeatherMap.h"

struct MemoryStruct {
    size_t size;
    char *memory;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    const size_t realSize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realSize + 1);
    LE_ASSERT(ptr);

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realSize);
    mem->size += realSize;
    mem->memory[mem->size] = 0;

    return realSize;
}

json_t *OpenWeatherMapGet(double latitude, double longitude, const char *apiKey)
{
    json_t *ret = NULL;
    char url[256];
    int res = snprintf(
        url,
        sizeof(url),
        "https://api.openweathermap.org/data/2.5/weather?appid=%s&mode=json&units=metric&lat=%f&lon=%f",
        apiKey,
        latitude,
        longitude);
    LE_ASSERT(res >= 0);
    LE_ASSERT(res < sizeof(url));

    CURL *curl = curl_easy_init();
    LE_ASSERT(curl);
    LE_ASSERT(curl_easy_setopt(curl, CURLOPT_URL, url) == CURLE_OK);
    /* send all data to this function  */
    LE_ASSERT(curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback) == CURLE_OK);
    /* we pass our 'chunk' struct to the callback function */
    struct MemoryStruct chunk;
    chunk.memory = malloc(1);  /* will be grown as needed by the realloc above */
    chunk.size = 0;
    LE_ASSERT(curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk) == CURLE_OK);

    CURLcode curlRes = curl_easy_perform(curl);
    if(curlRes != CURLE_OK)
    {
        LE_ERROR("curl_easy_perform() failed: %s", curl_easy_strerror(res));
        goto cleanup;
    }

    json_error_t error;
    size_t flags = 0;
    ret = json_loads(chunk.memory, flags, &error);
    if (!ret)
    {
        LE_ERROR("Failed to parse response as json: %s", (const char *)chunk.memory);
        goto cleanup;
    }

cleanup:
    free(chunk.memory);
    curl_easy_cleanup(curl);
    return ret;
}


COMPONENT_INIT
{
    LE_FATAL_IF(curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK, "Failed to initialize libcurl");
}

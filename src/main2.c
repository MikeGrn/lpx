#include <stdio.h>
#include <inttypes.h>
#include "list.h"
#include "lpxstd.h"
#include "stream_storage.h"

int main() {
    Storage *s;
    storage_open("/home/azhidkov/tmp/lpx-out", &s);
    FrameMeta **pf = 0;
    uint32_t frames_cnt = 0;
    storage_read_stream_idx(s, "1", &pf, &frames_cnt);
    for (int i = 0; i < frames_cnt; i++) {
        printf("%d,%d,%" PRId64 ",%" PRId64 "\n", pf[i]->offset, pf[i]->size,
               pf[i]->start_time, pf[i]->end_time);
        free(pf[i]);
    }
    free(pf);

    uint8_t* buf = 0;
    size_t len = 0;
    storage_read_frame(s, "1", 1, &buf, &len);
    printf("%c %ld\n", buf[0], len);
    free(buf);
    storage_close(s);
}

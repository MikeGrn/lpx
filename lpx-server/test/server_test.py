import sys, os, shutil
import urllib.request
import urllib.error
import urllib.response
import unittest
import struct


class TestLpxServer(unittest.TestCase):
    STORAGE_DIR = None

    @staticmethod
    def prepare_storage():
        for the_file in os.listdir(TestLpxServer.STORAGE_DIR):
            src_path = os.path.join(TestLpxServer.STORAGE_DIR, the_file)
            if os.path.isfile(src_path):
                os.unlink(src_path)
            elif os.path.isdir(src_path):
                shutil.rmtree(src_path)

        test_data_dir = "../../lpx-shared/test/test_dir"
        for the_file in os.listdir(test_data_dir):
            src_path = os.path.join(test_data_dir, the_file)
            dst_path = os.path.join(TestLpxServer.STORAGE_DIR, the_file)
            shutil.copytree(src_path, dst_path)

    def setUp(self):
        self.prepare_storage()

    def test_get_stream(self):
        response = urllib.request.urlopen("http://localhost:8888/stream?stream_time=1529488179412403")
        contents = response.read()
        response.close()
        self.check_archive_with_offset(contents, 0, 30)

    def test_get_stream_invalid_stream_time(self):
        try:
            urllib.request.urlopen("http://localhost:8888/stream?stream_time=invalid")
        except urllib.error.HTTPError as e:
            self.assertEqual(e.read().decode("ascii"), "invalid stream_time GET parameter")
            self.assertEqual(e.code, 400)

    def test_get_stream_offset(self):
        response = urllib.request.urlopen("http://localhost:8888/stream?stream_time=1529488179412403&offset=15")
        contents = response.read()
        response.close()
        self.check_archive_with_offset(contents, 15, 15)

    def test_get_stream_negative_offset(self):
        try:
            urllib.request.urlopen("http://localhost:8888/stream?stream_time=1529488179412403&offset=-1")
        except urllib.error.HTTPError as e:
            self.assertEqual(e.read().decode("ascii"), "invalid offset GET parameter")
            self.assertEqual(e.code, 400)

    def test_get_stream_invalid_offset(self):
        try:
            urllib.request.urlopen("http://localhost:8888/stream?stream_time=1529488179412403&offset=invalid")

        except urllib.error.HTTPError as e:
            self.assertEqual(e.read().decode("ascii"), "invalid offset GET parameter")
            self.assertEqual(e.code, 400)

    def test_get_stream_large_offset(self):
        response = urllib.request.urlopen("http://localhost:8888/stream?stream_time=1529488179412403&offset=30")
        contents = response.read()
        self.assertEqual(len(contents), 4)
        self.assertEqual(contents, b'\x00\x00\x00\x00')

    def test_stream_not_found(self):
        try:
            urllib.request.urlopen("http://localhost:8888/stream?stream_time=1529488179312403")
            self.fail("HTTPError with code 404 expected")
        except urllib.error.HTTPError as e:
            self.assertEqual(e.code, 404)

    def test_get_frames(self):
        response = urllib.request.urlopen(
            "http://localhost:8888/stream?stream_time=1529488179412403&"
            "frame_time=0&frame_time=630021&frame_time=1210079&frame_time=2581100&frame_time=2581101")
        contents = response.read()
        response.close()
        self.check_archive_with_frames(contents, [0, 7, 18, 29], 4)

    def test_get_frames_empty(self):
        response = urllib.request.urlopen(
            "http://localhost:8888/stream?stream_time=1529488179412403&frame_time=2581101")
        contents = response.read()
        response.close()
        self.check_archive_with_frames(contents, [], 0)

    def test_get_stream_frames_negative_offset(self):
        try:
            urllib.request.urlopen("http://localhost:8888/stream?stream_time=1529488179412403&frame_time=-1")
        except urllib.error.HTTPError as e:
            self.assertEqual(e.read().decode("ascii"), "invalid frame_time GET parameter")
            self.assertEqual(e.code, 400)

    def test_get_stream_frames_invalid_offset(self):
        try:
            urllib.request.urlopen("http://localhost:8888/stream?stream_time=1529488179412403&frame_time=invalid")
        except urllib.error.HTTPError as e:
            self.assertEqual(e.read().decode("ascii"), "invalid frame_time GET parameter")
            self.assertEqual(e.code, 400)

    def check_archive_with_offset(self, archive, frame_offset, expected_frames_cnt):
        frames_cnt = struct.unpack("<I", archive[0:4])
        self.assertEqual(frames_cnt[0], expected_frames_cnt)
        offset = 4
        for i in range(frame_offset, frame_offset + frames_cnt[0]):
            offset += self.check_frame(archive, offset, i)

    def check_archive_with_frames(self, archive, frames, expected_frames_cnt):
        frames_cnt = struct.unpack("<I", archive[0:4])
        self.assertEqual(frames_cnt[0], expected_frames_cnt)
        offset = 4
        for i in frames:
            offset += self.check_frame(archive, offset, i)

    def check_frame(self, archive, offset, frame_idx):
        index_len = len(str(frame_idx))
        name_len = index_len + len(".jpeg") + 1
        header = name_len + 8
        to = offset + header
        (name, size) = struct.unpack("<" + str(name_len) + "sQ", archive[offset:to])
        self.assertEqual(name.decode("ascii"), str(frame_idx) + ".jpeg\00")
        self.assertEqual(size, 921600)
        f = open("../../lpx-shared/test/test_dir/1529488179409/" + str(frame_idx) + ".jpeg", "rb")
        original = f.read()
        f.close()
        self.assertEqual(archive[to:to + size], original)
        return header + size

    def test_delete_stream(self):
        request = urllib.request.Request("http://localhost:8888/stream?stream_time=1529488179412403", method="DELETE")
        urllib.request.urlopen(request)
        self.assertEqual(sorted(os.listdir(TestLpxServer.STORAGE_DIR)), ["1529488204470","1529489555016"])

    def test_delete_not_existing_stream(self):
        try:
            request = urllib.request.Request("http://localhost:8888/stream?stream_time=1529488179412402", method="DELETE")
            urllib.request.urlopen(request)
            self.fail("404 expected")
        except urllib.error.HTTPError as e:
            self.assertEqual(e.code, 404)

    def test_delete_streams(self):
        request = urllib.request.Request("http://localhost:8888/streams", method="DELETE")
        urllib.request.urlopen(request)
        self.assertEqual(len(os.listdir(TestLpxServer.STORAGE_DIR)), 0)


if __name__ == '__main__':
    if len(sys.argv) > 1:
        TestLpxServer.STORAGE_DIR = sys.argv.pop()
    unittest.main()

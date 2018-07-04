import sys, os, shutil
import urllib.request
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
        self.prepare_storage()
        url = urllib.request.urlopen("http://localhost:8888/stream?stream_time=1529488179412403", timeout = 1000)
        contents = url.read()
        print(str(len(contents)))
        url.close()
        self.check_archive(contents, 30)

    def check_archive(self, archive, expected_frames_cnt):
        frames_cnt = struct.unpack("<I", archive[0:4])
        self.assertEqual(frames_cnt[0], expected_frames_cnt)
        offset = 4
        for i in range(0, frames_cnt[0]):
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
        for j in range(0, 921600):
            self.assertTrue(offset + header + j < len(archive), str(frame_idx) + " : " + str(j) + " : " + str(offset + header + j) + " : " + str(offset))
            self.assertEqual(archive[offset + header + j],  original[j])
        return header + size


if __name__ == '__main__':
    if len(sys.argv) > 1:
        TestLpxServer.STORAGE_DIR = sys.argv.pop()
    unittest.main()

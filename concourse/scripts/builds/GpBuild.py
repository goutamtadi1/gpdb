import subprocess
from GpdbBuildBase import GpdbBuildBase


class GpBuild(GpdbBuildBase):
    def __init__(self, mode):
        GpdbBuildBase.__init__(self)
        self.mode = 'on' if mode == 'orca' else 'off'

    def configure(self):
        return subprocess.call(["./configure",
                                "--enable-mapreduce",
                                "--with-gssapi",
                                "--with-perl",
                                "--with-libxml",
                                "--with-python",
                                "--disable-gpcloud",
                                "--prefix=/usr/local/gpdb"], cwd="gpdb_src")

    @staticmethod
    def create_demo_cluster():
        return subprocess.call([
            "runuser gpadmin -c \"source /usr/local/gpdb/greenplum_path.sh \
            && make create-demo-cluster DEFAULT_QD_MAX_CONNECT=150\""], cwd="gpdb_src/gpAux/gpdemo", shell=True)

    @staticmethod
    def export_ld_library_path_to_greenplum_path():
        return subprocess.call(
            "printf '\nLD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib\nexport \
            LD_LIBRARY_PATH' >> /usr/local/gpdb/greenplum_path.sh", shell=True)

    def install_check(self, option='good'):
        # status = self.export_ld_library_path_to_greenplum_path()
        # if status:
        #     return status
        status = self.create_demo_cluster()
        if status:
            return status
        if option == 'world':
            return self.run_install_check_with_command('make installcheck-world')
        else:
            return self.run_install_check_with_command('make -C src/test installcheck-good')

    def run_install_check_with_command(self, make_command):
        return subprocess.call([
            "runuser gpadmin -c \"source /usr/local/gpdb/greenplum_path.sh \
            && source gpAux/gpdemo/gpdemo-env.sh && PGOPTIONS='-c optimizer={0}' \
            {1} \"".format(self.mode, make_command)], cwd="gpdb_src", shell=True)

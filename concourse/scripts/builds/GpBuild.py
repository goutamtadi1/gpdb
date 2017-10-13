import subprocess
from GpdbBuildBase import GpdbBuildBase

class GpBuild(GpdbBuildBase):
    def __init__(self, mode):
        self.mode = 'on' if mode == 'orca' else 'off'

    def configure(self):
        subprocess.call(["g++","--version"])
        subprocess.call(["gcc","--version"])
        return subprocess.call(["./configure",
                                "--enable-mapreduce",
                                "--with-gssapi",
                                "--with-perl",
                                "--with-libxml",
                                "--with-python",
                                "--disable-gpcloud",
                                "--prefix=/usr/local/gpdb"], cwd="gpdb_src")

    def icg(self):
        status = self.export_Ld_Library_Path_to_Greenplum_Path()
        if status:
            return status
        status = self.create_Demo_Cluster()
        if status:
            return status
        return subprocess.call([
            "runuser gpadmin -c \"source /usr/local/gpdb/greenplum_path.sh \
            && source gpAux/gpdemo/gpdemo-env.sh && PGOPTIONS='-c optimizer={0}' \
            make -C src/test installcheck-good\"".format(self.mode)], cwd="gpdb_src", shell=True)

    def icw(self):
        status = self.export_Ld_Library_Path_to_Greenplum_Path()
        if status:
            return status
        status = self.create_Demo_Cluster()
        if status:
            return status
        return subprocess.call([
            "runuser gpadmin -c \"source /usr/local/gpdb/greenplum_path.sh \
            && source gpAux/gpdemo/gpdemo-env.sh && PGOPTIONS='-c optimizer={0}' \
            make installcheck-world\"".format(self.mode)], cwd="gpdb_src", shell=True)

    def create_Demo_Cluster(self):
        return subprocess.call([
            "runuser gpadmin -c \"source /usr/local/gpdb/greenplum_path.sh \
            && make create-demo-cluster DEFAULT_QD_MAX_CONNECT=150\""], cwd="gpdb_src/gpAux/gpdemo", shell=True)

    def export_Ld_Library_Path_to_Greenplum_Path(self):
        return subprocess.call(
            "printf '\nLD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib\nexport \
            LD_LIBRARY_PATH' >> /usr/local/gpdb/greenplum_path.sh", shell=True)
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include "NinePea.h"
#include "v4l.h"
#include "jpeg.h"

Fcall ofcall;
char errstr[64];
char Snone[] = "none";
char Sroot[] = "/";
char Sjpeg[] = "jpeg";
unsigned long long last = 0;
int jpeglen;

enum {
	Qroot = 0,
	Qjpeg,
	QNUM,
};

/* 9p handlers */

Fcall*
fs_attach(Fcall *ifcall) {
	ofcall.qid.type = QTDIR | QTTMP;
	ofcall.qid.version = 0;
	ofcall.qid.path = Qroot;

	fs_fid_add(ifcall->fid, Qroot);

	return &ofcall;
}

Fcall*
fs_walk(Fcall *ifcall) {
	unsigned long path;
	struct hentry *ent = fs_fid_find(ifcall->fid);
	int i;

	if (!ent) {
		ofcall.type = RError;
		ofcall.ename = Enofile;

		return &ofcall;
	}

	path = ent->data;

	for (i = 0; i < ifcall->nwname; i++) {
		switch(ent->data) {
		case Qroot:
			if (!strcmp(ifcall->wname[i], "jpeg")) {
				ofcall.wqid[i].type = QTFILE;
				ofcall.wqid[i].version = 0;
				ofcall.wqid[i].path = path = Qjpeg;
			} 
			else if (!strcmp(ifcall->wname[i], ".")) {
				ofcall.wqid[i].type = QTDIR;
				ofcall.wqid[i].version = 0;
				ofcall.wqid[i].path = path = Qroot;
			} 
			else {
				ofcall.type = RError;
				ofcall.ename = Enofile;
				return &ofcall;
			}
			break;
		default:
			ofcall.type = RError;
			ofcall.ename = Enofile;

			return &ofcall;
			break;
		}
	}

	ofcall.nwqid = i;

	if (fs_fid_find(ifcall->newfid) != NULL) {
		ofcall.type = RError;
		strcpy(errstr, "new fid exists");
		ofcall.ename = errstr;
		return &ofcall;
	}

	fs_fid_add(ifcall->newfid, path);

	return &ofcall;
}

Fcall*
fs_stat(Fcall *ifcall) {
	struct hentry *ent;

	if ((ent = fs_fid_find(ifcall->fid)) == NULL) {
		ofcall.type = RError;
		ofcall.ename = Enofile;

		return &ofcall;
	}

	ofcall.stat.qid.type = QTTMP;
	ofcall.stat.mode = 0666 | DMTMP;
	ofcall.stat.atime = ofcall.stat.mtime = ofcall.stat.length = 0;
	ofcall.stat.uid = Snone;
	ofcall.stat.gid = Snone;
	ofcall.stat.muid = Snone;

	switch (ent->data) {
	case Qroot:
		ofcall.stat.qid.type |= QTDIR;
		ofcall.stat.qid.path = Qroot;
		ofcall.stat.mode |= 0111 | DMDIR;
		ofcall.stat.name = Sroot;
		break;
	case Qjpeg:
		ofcall.stat.qid.path = Qjpeg;
		ofcall.stat.name = Sjpeg;
		break;
	}

	return &ofcall;
}

Fcall*
fs_clunk(Fcall *ifcall) {
	fs_fid_del(ifcall->fid);

	return ifcall;
}

Fcall*
fs_open(Fcall *ifcall) {
	struct hentry *cur = fs_fid_find(ifcall->fid);
	struct timeval tv;
	unsigned long long t;

	if (cur == NULL) {
		ofcall.type = RError;
		ofcall.ename = Enofile;

		return &ofcall;
	}

	ofcall.qid.type = QTFILE;
	ofcall.qid.path = cur->data;

	if (cur->data == Qroot)
		ofcall.qid.type = QTDIR;
	if (cur->data == Qjpeg) {
		gettimeofday(&tv, NULL);
		t = tv.tv_sec * 1000000 + tv.tv_usec;
		if ((t - last) > 66000) {
			read_frame();
			jpeglen = compressjpg(RGB, 640, 480);
			last = t;
		}
		cur->aux = malloc(jpeglen);
		memcpy(cur->aux, RGB, jpeglen);
		cur->len = jpeglen;
	}

	return &ofcall;
}

Fcall*
fs_read(Fcall *ifcall, unsigned char *out) {
	struct hentry *cur = fs_fid_find(ifcall->fid);
	Stat stat;
	char tmpstr[32];
	unsigned int i;
	unsigned long value;

	if (cur == NULL) {
		ofcall.type = RError;
		ofcall.ename = Enofile;
	}
	else if (((unsigned long)cur->data) == Qroot) {
		if (ifcall->offset == 0) {
			stat.type = 0;
			stat.dev = 0;
			stat.qid.type = QTFILE;
			stat.mode = 0666;
			stat.atime = 0;
			stat.mtime = 0;
			stat.length = 0;

			stat.qid.path = Qjpeg;
			stat.name = Sjpeg;
			stat.uid = Snone;
			stat.gid = Snone;
			stat.muid = Snone;
			ofcall.count = putstat(out, 0, &stat);
		}
	}
	else if (((unsigned long)cur->data) == Qjpeg) {
		i = cur->len - ifcall->offset;
		if (i > MAX_IO)
			i = MAX_IO;
		if (i > 0)
			memcpy(out, &((unsigned char*)cur->aux)[ifcall->offset], i);
		ofcall.count = i;
	}
	else {
		ofcall.type = RError;
		ofcall.ename = Enofile;
	}

	return &ofcall;
}

Fcall*
fs_create(Fcall *ifcall) {
	ofcall.type = RError;
	ofcall.ename = Eperm;

	return &ofcall;
}

Fcall*
fs_write(Fcall *ifcall, unsigned char *in) {
	struct hentry *cur = fs_fid_find(ifcall->fid);
	char *ep = &in[ifcall->count];

	ofcall.count = ifcall->count;

	if (cur == NULL) {
		ofcall.type = RError;
		ofcall.ename = Enofile;
	}
	else if (((unsigned long)cur->data) == Qroot) {
		ofcall.type = RError;
		ofcall.ename = Eperm;
	}
	else {
		ofcall.type = RError;
		ofcall.ename = Eperm;
	}

	return &ofcall;
}

Fcall*
fs_remove(Fcall *ifcall) {
	ofcall.type = RError;
	ofcall.ename = Eperm;

	return &ofcall;
}

Fcall*
fs_flush(Fcall *ifcall) {
	return ifcall;
}

Fcall*
fs_wstat(Fcall *ifcall) {
	return ifcall;
}

Callbacks callbacks;

void
sysfatal(int code)
{
	fprintf(stderr, "sysfatal: %d\n", code);
	exit(code);
}

int main(int argc, char **argv) {
	fs_fid_init(64);

	// this is REQUIRED by proc9p (see below)
	callbacks.attach = fs_attach;
	callbacks.flush = fs_flush;
	callbacks.walk = fs_walk;
	callbacks.open = fs_open;
	callbacks.create = fs_create;
	callbacks.read = fs_read;
	callbacks.write = fs_write;
	callbacks.clunk = fs_clunk;
	callbacks.remove = fs_remove;
	callbacks.stat = fs_stat;
	callbacks.wstat = fs_wstat;

	unsigned char *msg = malloc(MAX_MSG+1);
	unsigned int msglen = 0;
	unsigned int r = 0;

	if (argc == 2)
		dev_name = strdup(argv[1]);
	else
		dev_name = strdup("/dev/video0");

	open_device();
	init_device();
	start_capturing();

	for(;;){
		unsigned long i;

		while (r < 5) {
			msg[r++] = getchar();
		}

		i = 0;
		get4(msg, i, msglen);

		// sanity check
		if (msg[i] & 1 || msglen > MAX_MSG || msg[i] < TVersion || msg[i] > TWStat) {
			sysfatal(3);
		}

		while (r < msglen) {
			msg[r++] = getchar();
		}

		memset(&ofcall, 0, sizeof(ofcall));

		// proc9p accepts valid 9P msgs of length msglen,
		// processes them using callbacks->various(functions);
		// returns variable out's msglen
		r = MAX_MSG;
		msglen = proc9p(msg, msglen, &callbacks);
		if (r != MAX_MSG)
			msg = realloc(msg, MAX_MSG+1);

		write(1, msg, msglen);
		fflush(stdout);

		r = msglen = 0;
	}

	stop_capturing();
	uninit_device();
	close_device();
}


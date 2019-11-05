extern char *dev_name;
extern unsigned char *RGB;

void open_device();
void init_device();
void start_capturing();
void stop_capturing();
void uninit_device();
void close_device();

int read_frame();



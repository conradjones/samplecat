
#ifdef __audtioner_c__
struct _auditioner
{
	DBusGProxy*    proxy;
};
#endif

void auditioner_connect();
void auditioner_play(Sample*);
void auditioner_play_result(Result*);
void auditioner_play_path(const char*);
void auditioner_stop(Sample*);
void auditioner_toggle(Sample*);
void auditioner_play_all();

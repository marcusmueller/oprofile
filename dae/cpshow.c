#include "cprofiled.h"

struct cpd_sample {
	u16 pid;
	u16 count;
	u8 type;
	u8 um;
	u16 image_nr;
	u16 process_image_nr; 
	u32 offset;
} __attribute__((__packed__));
 
struct cpd_sample cpd_samples[1000000];
 
char *images[256] = { NULL, }; 
 
inline void show_sample(struct cpd_sample *s)
{
	if (!s->pid)
		printf("[kernel] ");
	else
		printf("[%.6d] ",s->pid);

	printf("type 0x%.2x:%.2x offset 0x%.8x count %.6d:  ",s->type,s->um,s->offset,s->count);

	if (!s->pid)
		printf("(kernel) (%d,%d)\n",s->process_image_nr,s->image_nr);
	else
		printf("%s[%.3d], map %s[%.3d]\n",images[s->process_image_nr],s->process_image_nr,
			images[s->image_nr], s->image_nr);
}
 
int main (int argc, char *argv[])
{
	FILE *fp;
	int i=0; 

	fp = cpd_open_file(argv[2],"r");

	images[0] = "kernel";
	i=1;
	while (1) {
		images[i] = cpd_get_line(fp);
		i++;
		if (feof(fp))
			break;
	}

	fp = cpd_open_file(argv[1],"r");

	i=0; 
	while (1) {
		cpd_samples[i].pid = cpd_read_u16(fp);
		cpd_samples[i].count = cpd_read_u16(fp);
		cpd_samples[i].type = cpd_read_u8(fp);
		cpd_samples[i].um = cpd_read_u8(fp);
		cpd_samples[i].image_nr = cpd_read_u16(fp);
		cpd_samples[i].process_image_nr = cpd_read_u16(fp); 
		cpd_samples[i].offset = cpd_read_u32(fp);
		show_sample(&cpd_samples[i]); 
		i++; 
		if (feof(fp))
			break;
	}

	return 0; 
} 

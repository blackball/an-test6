unsigned char label_image_char(unsigned char* source, int W, int H, unsigned char* dest);
int  label_image_int(unsigned char* source, int W, int H, int* dest);

struct label_info {
	int label;
	int xlow;
	int xhigh;
	int ylow;
	int yhigh;
	int mass;
	float xcenter;
	float ycenter;

	int value;
};
typedef struct label_info label_info;

label_info* label_image_super(unsigned char* source, int W, int H, int* dest, int* N);

label_info* label_image_super_2(unsigned char* source, int W, int H, int* dest, int* N);

inline float roundness(label_info* label, int* labelim, int WW);

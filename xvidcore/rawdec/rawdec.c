/*
   rawdec: raw mpeg-4 bitstream decoder
   (c)2001-2003 peter ross <pross@xvid.org>

*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xvid.h>


const static char * xvid_err(int err)
{
	if (err == XVID_ERR_MEMORY) return "mem";
	if (err == XVID_ERR_FORMAT)	return "format";
	if (err == XVID_ERR_VERSION) return "version";
	return "unknown";
}


/* returns true if the string contains scanf %'s chars
   (ignores %% sequences) */

static int detect_sscanf(const char *s)
{
    int percent = 0;
    while(*s){
        if (percent){
            if (*s!='%') 
                return 1;
            percent=0;
        }
        if (*s=='%') 
            percent = 1;
        s++;
    }
    return 0;
}


int main(int argc, char **argv)
{
	char tmp[1024];
	FILE * fbs, *fpgm;
    int fscanf;
	void * bs;
    int quit;
	int length;
	int pos;
	int frameno;
	int result;
	int width, height;
    
	xvid_gbl_info_t info;
	xvid_gbl_init_t init;
	xvid_dec_create_t create;
	xvid_dec_frame_t frame;
	xvid_dec_stats_t stats;

	memset(&info, 0, sizeof(info));
	info.version = XVID_VERSION;

	result = xvid_global(0, XVID_GBL_INFO, &info, 0);
	if (result < 0)
	{

		printf("xvid_gbl_info error: %s\n", xvid_err(result));
		return EXIT_FAILURE;
	}
	printf("xvidcore version %d.%d.%d (\"%s\")\n",
		XVID_MAJOR(info.actual_version),
		XVID_MINOR(info.actual_version),
		XVID_PATCH(info.actual_version),
		info.build);
	printf("cpu_flags:");
	if ((info.cpu_flags & XVID_CPU_ASM))	printf(" ASM");
	if ((info.cpu_flags & XVID_CPU_MMX))	printf(" MMX");
	if ((info.cpu_flags & XVID_CPU_MMXEXT))	printf(" MMXEXT");
	if ((info.cpu_flags & XVID_CPU_3DNOW))	printf(" 3DNOW");
	if ((info.cpu_flags & XVID_CPU_3DNOWEXT))	printf(" 3DNOWEXT");
	if ((info.cpu_flags & XVID_CPU_SSE))	printf(" SSE");
	if ((info.cpu_flags & XVID_CPU_SSE2))	printf(" SSE2");
	printf("\n");
	printf("num_threads: %i\n", info.num_threads);

	if (argc != 3 && argc != 5)
	{
		fprintf(stderr, 
			"\nusage: rawdec bitstream outfile [width height]\n"
			"\n"
			"eg: rawdec foo.m4v [-|out.pgm|out%%05i.pgm]\n");

		return -1;
	}

	/* open bitstream file & get length */

	fbs = fopen(argv[1], "rb");
	if (fbs == NULL)
	{
	      fprintf(stderr, "fatal: \"%s\" not found", argv[1]);
	      return -1;
	}

	if (argc == 5)
	{
        width = atoi(argv[3]);
        height = atoi(argv[4]);
	}
	else
	{
		width = height = 0;	/* auto detect */
	}

	fseek(fbs, 0, SEEK_END);
	length = ftell(fbs);
	fseek(fbs, 0, SEEK_SET);

	/* XXX: load entire bitstream file into memory */

	bs = malloc(length);
	if (bs == NULL)
	{
		fprintf(stderr, "fatal: bitstream malloc failed\n");
		return -1;
	}

	fread(bs, length, 1, fbs);
	fclose(fbs);


    /* otuput file stuff */
    fscanf = detect_sscanf(argv[2]);
    if (argv[2][0] != '-' && !fscanf)
    {
		fpgm = fopen(argv[2], "wb");
		if (fpgm == NULL){
			fprintf(stderr,"aborting: \"%s\" write error\n", argv[2]);
            free(bs);
            return EXIT_FAILURE;
		}
    }


	/* init globals 
		note: init.cpu_flags is optional
	*/

	memset(&init, 0, sizeof(init));
	init.version = XVID_VERSION;	
	//init.cpu_flags = XVID_CPU_FORCE;
	result = xvid_global(0, XVID_GBL_INIT, &init, NULL);
	if (result < 0)
	{
		free(bs);
		printf("xvid_gbl_init error: %s\n", xvid_err(result));
		return EXIT_FAILURE;
	}


	/* create decoder instance */

	memset(&create, 0, sizeof(create));
	create.version = XVID_VERSION;
	create.width = width;
	create.height = height;
	result = xvid_decore(0, XVID_DEC_CREATE, &create, NULL);
	if (result < 0)
	{
		free(bs);
		fprintf(stderr, "xvid_dec_create error: %s\n", xvid_err(result));
		return EXIT_FAILURE;
	}


	/* init other struct; we only have to do this once */
	memset(&frame, 0, sizeof(frame));
	frame.version = XVID_VERSION;

	memset(&stats, 0, sizeof(stats));
	stats.version = XVID_VERSION;

	
	/* decode frames ...  */
	quit = 0;
	pos = 0;
	frameno = 0;
	while (!quit)
	{
		int y;

		if (pos < length)
		{
			frame.bitstream = (void*)((unsigned long)bs + pos);
			frame.length = length - pos;
		}else{
			printf("**FLUSH LAST FRAME\n");
			frame.length = -1;	/* flush last frame */
			quit = 1;
		}
		/* xvid will return pointers to it's internal image buffers */
		frame.output.csp = XVID_CSP_INTERNAL;	
			
		result = xvid_decore(create.handle, XVID_DEC_DECODE, &frame, &stats);
        fprintf(stdout," #%i  [%i/%i] length=%i\n", frameno, pos, length, frame.length); fflush(stderr);
		if (result < 0)
		{
			fprintf(stderr, "xvid_dec_decode error: %s\n", xvid_err(result));
			break;
		}

		/* increase position by the ammount consumed for this frame */
		pos += result;


		if (stats.type == XVID_TYPE_NOTHING)	/* no output */
		{
			continue;
		}

		if (stats.type == XVID_TYPE_VOL)		/* vol/resize */
		{
			printf("**RESIZE**  %i,%i\n", stats.data.vol.width, stats.data.vol.height);
			width = stats.data.vol.width;
			height = stats.data.vol.height;
			printf("%i\n", frame.length);
			continue;
		}

		/* write output image to pgm file */
		if (argv[2][0] == '-')
		{
			fpgm = stdout;
		}else if (fscanf) {
		    sprintf(tmp,argv[2], frameno);
		    fpgm = fopen(tmp, "wb");
		    if (fpgm == NULL) {
			    fprintf(stderr,"aborting: \"%s\" write error\n", tmp);
			    break;
		    }
		}

		sprintf(tmp, "P5\n%i %i\n255\n", width, height * 3 / 2);
		fwrite(tmp, strlen(tmp) , 1, fpgm);
		
		for (y = height; y; y--)        {
			fwrite(frame.output.plane[0], width, 1, fpgm);
			frame.output.plane[0] = (void*)((char *)frame.output.plane[0] + frame.output.stride[0]);
		}

		for (y = height >> 1; y; y--) {
			fwrite(frame.output.plane[1], width / 2, 1, fpgm);
			frame.output.plane[1] = (void*)((char *)frame.output.plane[1] + frame.output.stride[1]);

			fwrite(frame.output.plane[2], width / 2, 1, fpgm);
			frame.output.plane[2] = (void*)((char *)frame.output.plane[2] + frame.output.stride[2]);
		}

		if (argv[2][0] != '-' && fscanf) {
			fclose(fpgm);
		}


		/* finally, increment frame num */
		frameno++;
	}


	/* cleanup & exit */

    if (argv[2][0] != '-' && !fscanf)
    {
        fclose(fpgm);
    }

	free(bs);

	result = xvid_decore(create.handle, XVID_DEC_DESTROY, NULL, NULL);
	if (result < 0)
	{
		fprintf(stderr, "xvid_dec_destroy error: %s\n", xvid_err(result));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
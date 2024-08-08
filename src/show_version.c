#include "show_version.h"
// show feature list according to release notes
char g_ac_changelist[]=
    "\n\t1.the first version";
char g_ac_version[] = "release V0.1.0";


void show_version(void)
{
    fprintf(stdout,"UT %s \n", g_ac_version); 
    fprintf(stdout,"Features: %s\n", g_ac_changelist);
        
    return;
}

/* Parse the argument given in the command line of the application */
static int
parse_private_args(char *arg)
{
    switch (arg[1])
    {
        case 'v':
        case 'V':            
            return SHOW_VERSION;
    }
    
    return 0;
}

int read_command_line_params(uint32_t argc, char *argv[])
{
    int i_error     = 0;
    uint32_t ui_index    = 1;


    for (ui_index = 1; ui_index < argc; ++ui_index)
    {
        if ((i_error = parse_private_args(argv[ui_index])) != 0)
        {
            switch (i_error)
            {
            case SHOW_VERSION:
                 show_version();
				 exit(0);
                break;
            default :
                break;
            }
            return i_error;
        }
    }

    return 0;
}



#include "../include/helpers.h"

void construct_file_path(const char *base_path, const char *relative_path, char *filepath, char *dir_name)
{
    char *filename = strstr(base_path, relative_path);

    if (filename == NULL)
    {
        printf("Invalid file path: %s\n", base_path);
        exit(1);
    }
    filename += strlen(relative_path) + 1; // Skip the dir_path
    snprintf(filepath, MAX_PATH_LEN, "%s/%s", dir_name, filename);
}

int lock_file(int fd)
{
    struct flock fl;
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;

    if (fcntl(fd, F_SETLK, &fl) == -1)
    {
        return -1;
    }

    return 0;
}

int unlock_file(int fd)
{
    struct flock fl;
    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;

    if (fcntl(fd, F_SETLK, &fl) == -1)
    {
        perror("Failed to release the file lock");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

int check_file_lock(const char *filename)
{
    int fd = open(filename, O_RDONLY);
    if (fd == -1)
    {
        perror("Failed to open the file");
        return -1;
    }

    struct flock fl;
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;

    if (fcntl(fd, F_GETLK, &fl) == -1)
    {
        perror("Failed to check the file lock");
        close(fd);
        return -1;
    }

    close(fd);

    if (fl.l_type != F_UNLCK)
    {
        return 1; // File is locked
    }

    return 0; // File is not locked
}

void block_thread_signals(sigset_t *signal_set)
{
    sigemptyset(signal_set);
    sigaddset(signal_set, SIGINT);
    sigaddset(signal_set, SIGSTOP);
    /* sigaddset(signal_set, SIGTSTP); */
    sigaddset(signal_set, SIGTERM);
    sigaddset(signal_set, SIGQUIT);
    sigaddset(signal_set, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, signal_set, NULL);
}

void check_directory(const char *directory)
{
    // Check if the directory exists
    struct stat dir_stat;
    if (stat(directory, &dir_stat) == -1)
    {
        // Directory does not exist, attempt to create it
        printf("The specified directory does not exist. Creating...\n");
        if (mkdir(directory, 0777) == -1)
        {
            perror("Failed to create the directory");
            exit(1);
        }
        printf("Directory created successfully.\n");
    }
    else if (!S_ISDIR(dir_stat.st_mode))
    {
        // Directory does not exist, attempt to create it
        printf("Entered file is not a directory...\nExiting...\n");
        exit(1);
    }
}

int create_nested_directory(const char *path)
{
    // Check if directory already exists
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
    {
        // Directory already exists
        return 1;
    }

    // Try creating the directory
    int result = mkdir(path, 0777);
    if (result == 0)
    {
        // Directory created successfully
        return 1;
    }
    else
    {
        // Directory creation failed
        if (errno == ENOENT)
        {
            // Parent directory doesn't exist, create it recursively
            char *parent = strdup(path);
            char *last_separator = strrchr(parent, '/');
            if (last_separator != NULL)
            {
                *last_separator = '\0'; // Cut off the last component
                if (create_nested_directory(parent))
                {
                    // Parent directory created, try creating the directory again
                    result = mkdir(path, 0777);
                    if (result == 0)
                    {
                        // Directory created successfully
                        free(parent);
                        return 1;
                    }
                }
            }
            free(parent);
        }
        return 0;
    }
}
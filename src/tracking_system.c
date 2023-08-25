#include "../include/tracking_system.h"

void init_tracking_system(tracking_system_t *tracking_system, const char *dir_path, char *log_file_path)
{
    strncpy(tracking_system->dir_path, dir_path, MAX_PATH_LEN);
    tracking_system->num_tracked_files = 0;
    tracking_system->tracked_files = NULL;
    pthread_mutex_init(&tracking_system->tracking_mutex, NULL);
    tracking_system->signal_received = 0;
    tracking_system->shut_down = 0;
    tracking_system->signal_str = NULL;
    if (log_file_path != NULL)
        strncpy(tracking_system->log_file_path, log_file_path, MAX_PATH_LEN);
    else
        memset(tracking_system->log_file_path, 0, MAX_PATH_LEN);

    fill_tracking_system(tracking_system);
}

void fill_tracking_system(tracking_system_t *tracking_system)
{
    fill_tracking_system_helper(tracking_system, tracking_system->dir_path);
}

void fill_tracking_system_helper(tracking_system_t *tracking_system, const char *dir_path)
{
    // Open the directory for reading
    DIR *dir = opendir(dir_path);
    if (dir == NULL)
    {
        return;
    }

    // Read the directory entries
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        // Ignore "." and ".." entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        // Construct the full path of the entry
        char entry_path[MAX_PATH_LEN + MAX_FILENAME_LEN];
        snprintf(entry_path, sizeof(entry_path), "%s/%s", dir_path, entry->d_name);

        if (strcmp(entry_path, tracking_system->log_file_path) == 0)
        {
            continue;
        }

        // Stat the entry to get file information
        struct stat file_stat;
        if (stat(entry_path, &file_stat) != 0)
        {
            continue;
        }

        // Create a new tracked_file_t struct
        tracked_file_t new_tracked_file;
        strncpy(new_tracked_file.path, entry_path, MAX_PATH_LEN);
        new_tracked_file.modified_time = file_stat.st_mtime;
        new_tracked_file.status = STABLE; // Initially set as STABLE
        new_tracked_file.is_dir = S_ISDIR(file_stat.st_mode);

        // Allocate memory for a new temporary array
        tracked_file_t *temp = malloc(sizeof(tracked_file_t) * (tracking_system->num_tracked_files + 1));
        if (temp == NULL)
        {
            perror("Error allocating memory");
            closedir(dir);
            return;
        }

        // Copy the existing tracked_files data to the new array
        memcpy(temp, tracking_system->tracked_files, sizeof(tracked_file_t) * tracking_system->num_tracked_files);

        // Free the previous memory
        free(tracking_system->tracked_files);

        tracking_system->tracked_files = temp;
        tracking_system->tracked_files[tracking_system->num_tracked_files] = new_tracked_file;
        tracking_system->num_tracked_files++;

        // Recursively fill the tracking system if the entry is a directory
        if (S_ISDIR(file_stat.st_mode))
        {
            fill_tracking_system_helper(tracking_system, entry_path);
        }
    }

    // Close the directory
    closedir(dir);
}

void check_statuses(tracking_system_t *tracking_system)
{
    check_statuses_helper(tracking_system, tracking_system->dir_path);
}

void check_statuses_helper(tracking_system_t *tracking_system, const char *dir_path)
{
    // Open the directory for reading
    DIR *dir = opendir(dir_path);
    if (dir == NULL)
    {
        return;
    }

    // Read the directory entries
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        // Ignore "." and ".." entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        // Construct the full path of the entry
        char entry_path[MAX_PATH_LEN + MAX_FILENAME_LEN];
        snprintf(entry_path, sizeof(entry_path), "%s/%s", dir_path, entry->d_name);

        // Stat the entry to get file information
        struct stat file_stat;
        if (stat(entry_path, &file_stat) != 0)
        {
            continue;
        }

        pthread_mutex_lock(&tracking_system->tracking_mutex);

        if (strcmp(entry_path, tracking_system->log_file_path) == 0)
        {
            pthread_mutex_unlock(&tracking_system->tracking_mutex);
            continue;
        }

        tracked_file_t *tracked_file = find_tracked_file(tracking_system, entry_path);

        if (tracked_file == NULL)
            add_tracked_file(tracking_system, entry_path, file_stat.st_mtime, S_ISDIR(file_stat.st_mode));
        else
            check_modification(tracked_file, file_stat.st_mtime);

        pthread_mutex_unlock(&tracking_system->tracking_mutex);

        // Recursively process subdirectories
        if (S_ISDIR(file_stat.st_mode))
        {
            check_statuses_helper(tracking_system, entry_path);
        }
    }

    // Close the directory
    closedir(dir);
}

void check_deletion(tracking_system_t *tracking_system)
{
    int i;
    tracked_file_t *tracked_file = NULL;
    pthread_mutex_lock(&tracking_system->tracking_mutex);
    for (i = 0; i < tracking_system->num_tracked_files; ++i)
    {
        tracked_file = &tracking_system->tracked_files[i];
        if (access(tracked_file->path, F_OK) != 0)
        {
            tracked_file->status = DELETED;
        }
    }
    pthread_mutex_unlock(&tracking_system->tracking_mutex);
}

// maybe no need to use this function
void remove_tracked_file(tracking_system_t *tracking_system, const char *file_path)
{
    int i;
    for (i = 0; i < tracking_system->num_tracked_files; i++)
    {
        if (strcmp(tracking_system->tracked_files[i].path, file_path) == 0)
        {
            // Found the file, now remove it from the array
            for (int j = i; j < tracking_system->num_tracked_files - 1; j++)
            {
                tracking_system->tracked_files[j] = tracking_system->tracked_files[j + 1];
            }
            tracking_system->num_tracked_files--;
            break;
        }
    }
}

tracked_file_t *find_tracked_file(tracking_system_t *tracking_system, const char *file_path)
{
    tracked_file_t *tracked_file = NULL;
    for (int i = 0; i < tracking_system->num_tracked_files; i++)
    {
        if (strcmp(tracking_system->tracked_files[i].path, file_path) == 0)
        {
            tracked_file = &tracking_system->tracked_files[i];
            break;
        }
    }
    return tracked_file;
}

void add_tracked_file(tracking_system_t *tracking_system, const char *file_path, time_t mtime, int is_dir)
{
    tracked_file_t new_tracked_file;
    strncpy(new_tracked_file.path, file_path, MAX_PATH_LEN);
    new_tracked_file.modified_time = mtime;
    new_tracked_file.status = CREATED;
    new_tracked_file.is_dir = is_dir;

    tracking_system->num_tracked_files++;
    tracking_system->tracked_files = realloc(tracking_system->tracked_files, sizeof(tracked_file_t) * tracking_system->num_tracked_files);
    tracking_system->tracked_files[tracking_system->num_tracked_files - 1] = new_tracked_file;
}

void update_tracking_system(tracking_system_t *tracking_system, char *filepath, request_status_t status)
{
    tracked_file_t new_file;
    new_file.status = STABLE;
    strcpy(new_file.path, filepath);
    struct stat file_stat;
    if (stat(filepath, &file_stat) != 0)
    {
        return;
    }

    new_file.is_dir = S_ISDIR(file_stat.st_mode);
    new_file.modified_time = file_stat.st_mtime;

    if (status == CREATE)
    {
        tracking_system->num_tracked_files++;
        tracking_system->tracked_files = realloc(tracking_system->tracked_files, tracking_system->num_tracked_files * sizeof(tracked_file_t));
        tracking_system->tracked_files[tracking_system->num_tracked_files - 1] = new_file;
    }
    else
    {
        tracked_file_t *file = find_tracked_file(tracking_system, new_file.path);
        if (file != NULL)
            file->modified_time = new_file.modified_time;
    }
}

void check_modification(tracked_file_t *tracked_file, time_t mtime)
{
    // If it is a directory no need to check modification
    if (tracked_file->is_dir)
        return;

    // Check if the file is modified
    if (mtime > tracked_file->modified_time)
    {
        tracked_file->status = UPDATED;
        tracked_file->modified_time = mtime;
    }
    else
    {
        tracked_file->status = STABLE;
    }
}

void tracking_system_set_signal(tracking_system_t *tracking_system, char *signal_str)
{
    pthread_mutex_lock(&(tracking_system->tracking_mutex));
    tracking_system->signal_received = 1;
    tracking_system->signal_str = signal_str;
    pthread_mutex_unlock(&(tracking_system->tracking_mutex));
}

int tracking_system_check_signal(tracking_system_t *tracking_system, int lock)
{
    int retval;
    if (lock == 1)
        pthread_mutex_lock(&(tracking_system->tracking_mutex));
    retval = tracking_system->signal_received;

    if (lock == 1)
        pthread_mutex_unlock(&(tracking_system->tracking_mutex));
    return retval;
}

void tracking_system_set_shutdown(tracking_system_t *tracking_system)
{
    pthread_mutex_lock(&(tracking_system->tracking_mutex));
    tracking_system->shut_down = 1;
    pthread_mutex_unlock(&(tracking_system->tracking_mutex));
}

int tracking_system_check_shutdown(tracking_system_t *tracking_system, int lock)
{
    int retval;
    if (lock == 1)
        pthread_mutex_lock(&(tracking_system->tracking_mutex));
    retval = tracking_system->shut_down;

    if (lock == 1)
        pthread_mutex_unlock(&(tracking_system->tracking_mutex));
    return retval;
}

void destroy_tracking_system(tracking_system_t *tracking_system)
{
    if (tracking_system != NULL)
    {
        if (tracking_system->tracked_files != NULL)
        {
            free(tracking_system->tracked_files);
        }

        // Destroy the mutex
        pthread_mutex_destroy(&tracking_system->tracking_mutex);
    }
}
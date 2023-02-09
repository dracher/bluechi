#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include "utils.h"

int bus_parse_property_string(sd_bus_message *m, const char *name, const char **value) {
        bool found = false;
        int r = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "{sv}");
        if (r < 0) {
                return r;
        }
        while ((r = sd_bus_message_enter_container(m, SD_BUS_TYPE_DICT_ENTRY, "sv")) > 0) {
                const char *member = NULL;
                const char *contents = NULL;

                r = sd_bus_message_read_basic(m, SD_BUS_TYPE_STRING, &member);
                if (r < 0) {
                        return r;
                }

                if (streq(name, member)) {
                        r = sd_bus_message_peek_type(m, NULL, &contents);
                        if (r < 0) {
                                return r;
                        }

                        if (!streq(contents, "s")) {
                                return -EIO; /* Wrong type */
                        }

                        r = sd_bus_message_enter_container(m, SD_BUS_TYPE_VARIANT, "s");
                        if (r < 0) {
                                return r;
                        }

                        r = sd_bus_message_read_basic(m, SD_BUS_TYPE_STRING, value);
                        if (r < 0) {
                                return r;
                        }

                        found = true;
                        r = sd_bus_message_exit_container(m);
                        if (r < 0) {
                                return r;
                        }
                } else {
                        r = sd_bus_message_skip(m, "v");
                        if (r < 0) {
                                return r;
                        }
                }

                r = sd_bus_message_exit_container(m);
                if (r < 0) {
                        return r;
                }

                if (found) {
                        break;
                }
        }
        if (r < 0) {
                return r;
        }

        r = sd_bus_message_exit_container(m);
        if (r < 0) {
                return r;
        }

        if (found) {
                return 0;
        }
        return -ENOENT;
}

UnitInfo *new_unit() {
        _cleanup_unit_ UnitInfo *unit = malloc0(sizeof(UnitInfo));
        if (unit == NULL) {
                return NULL;
        }

        unit->ref_count = 1;
        LIST_INIT(units, unit);

        return steal_pointer(&unit);
}

UnitInfo *unit_ref(UnitInfo *unit) {
        unit->ref_count++;
        return unit;
}

void unit_unref(UnitInfo *unit) {
        unit->ref_count--;
        if (unit->ref_count != 0) {
                return;
        }

        free(unit->node);
        free(unit->id);
        free(unit->description);
        free(unit->load_state);
        free(unit->active_state);
        free(unit->sub_state);
        free(unit->following);
        free(unit->unit_path);
        free(unit->job_type);
        free(unit->job_path);

        free(unit);
}

/*
 * Copied from systemd/bus-unit-util.c
 */
int bus_parse_unit_info(sd_bus_message *message, UnitInfo *u) {
        int r = 0;
        char *id = NULL, *description = NULL, *load_state = NULL, *active_state = NULL;
        char *sub_state = NULL, *following = NULL, *unit_path = NULL;
        int job_id = 0;
        char *job_type = NULL, *job_path = NULL;

        assert(message);
        assert(u);

        r = sd_bus_message_read(
                        message,
                        UNIT_INFO_STRUCT_TYPESTRING,
                        &id,
                        &description,
                        &load_state,
                        &active_state,
                        &sub_state,
                        &following,
                        &unit_path,
                        &job_id,
                        &job_type,
                        &job_path);

        if (r <= 0) {
                return r;
        }

        u->node = NULL;
        u->id = strdup(id);
        u->description = strdup(description);
        u->load_state = strdup(load_state);
        u->active_state = strdup(active_state);
        u->sub_state = strdup(sub_state);
        u->following = strdup(following);
        u->unit_path = strdup(unit_path);
        u->job_id = job_id;
        u->job_type = strdup(job_type);
        u->job_path = strdup(job_path);

        return r;
}

int bus_parse_unit_on_node_info(sd_bus_message *message, UnitInfo *u) {
        int r = 0;
        char *node = NULL, *id = NULL, *description = NULL, *load_state = NULL;
        char *active_state = NULL, *sub_state = NULL, *following = NULL, *unit_path = NULL;
        int job_id = 0;
        char *job_type = NULL, *job_path = NULL;
        assert(message);
        assert(u);

        r = sd_bus_message_read(
                        message,
                        NODE_AND_UNIT_INFO_STRUCT_TYPESTRING,
                        &node,
                        &id,
                        &description,
                        &load_state,
                        &active_state,
                        &sub_state,
                        &following,
                        &unit_path,
                        &job_id,
                        &job_type,
                        &job_path);

        if (r <= 0) {
                return r;
        }

        u->node = strdup(node);
        u->id = strdup(id);
        u->description = strdup(description);
        u->load_state = strdup(load_state);
        u->active_state = strdup(active_state);
        u->sub_state = strdup(sub_state);
        u->following = strdup(following);
        u->unit_path = strdup(unit_path);
        u->job_id = job_id;
        u->job_type = strdup(job_type);
        u->job_path = strdup(job_path);

        return r;
}

int assemble_object_path_string(const char *prefix, const char *name, char **res) {
        _cleanup_free_ char *escaped = bus_path_escape(name);
        if (escaped == NULL) {
                return -ENOMEM;
        }
        return asprintf(res, "%s/%s", prefix, escaped);
}

static char hexchar(int x) {
        static const char table[16] = "0123456789abcdef";
        return table[(unsigned int) x % sizeof(table)];
}

char *bus_path_escape(const char *s) {

        if (*s == 0) {
                return strdup("_");
        }

        char *r = malloc(strlen(s) * 3 + 1);
        if (r == NULL) {
                return NULL;
        }

        char *t = r;
        for (const char *f = s; *f; f++) {
                /* Escape everything that is not a-zA-Z0-9. We also escape 0-9 if it's the first character */
                if (!ascii_isalpha(*f) && !(f > s && ascii_isdigit(*f))) {
                        unsigned char c = *f;
                        *(t++) = '_';
                        *(t++) = hexchar(c >> 4U);
                        *(t++) = hexchar(c);
                } else {
                        *(t++) = *f;
                }
        }

        *t = 0;

        return r;
}

int bus_socket_set_no_delay(sd_bus *bus) {
        int fd = sd_bus_get_fd(bus);
        if (fd < 0) {
                return fd;
        }

        int flag = 1;
        int r = setsockopt(fd, SOL_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
        if (r < 0) {
                return -errno;
        }

        return 0;
}

int bus_socket_set_keepalive(sd_bus *bus) {
        int fd = sd_bus_get_fd(bus);
        if (fd < 0) {
                return fd;
        }

        int flag = 1;
        int r = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char *) &flag, sizeof(int));
        if (r < 0) {
                return -errno;
        }

        return 0;
}

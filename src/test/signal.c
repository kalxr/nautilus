#include <nautilus/nautilus.h>
#include <nautilus/process.h>
#include <nautilus/shell.h>
#include <nautilus/barrier.h>

#define DO_PRINT 1

#if DO_PRINT
#define PRINT(...) nk_vc_printf(__VA_ARGS__)
#else
#define PRINT(...)
#endif

nk_counting_barrier_t barrier;
nk_signal_action_t old_sig_act;
nk_signal_action_t older_sig_act;
volatile int sig_n = 0;


/* Create custom signal handler */
void sig_hand_hello_2(int sig_num);
nk_signal_action_t new_sig_act = {
    .handler = sig_hand_hello_2,
    .mask = {},
    .signal_flags = SIG_ACT_ONESHOT, 
};

void 
sig_hand_hello_2 (int sig_num)
{
    //SIGNAL_INFO("Hello World from signal %d.\n", sig_num);
    nk_vc_printf(".%d langis morf dlroW olleH\n", sig_num);
    if (sig_num == 17 || sig_num == 18) {
        if (sig_n < 1) {
            do_sigaction(17, &old_sig_act, &older_sig_act); 
            do_sigaction(18, &new_sig_act, &old_sig_act); 
            nk_counting_barrier(&barrier);
        }
        sig_n++;
    }
}


/* Thread that will receive signals */
void
sig_thread1 (void *in, void **out)
{
    nk_thread_t *me = get_cur_thread();  
    do_sigaction(17, &new_sig_act, &old_sig_act); 
    nk_counting_barrier(&barrier);
    while (1) {
        if (sig_n >= 2) {
            nk_vc_printf("Thread 1 exiting. Success!\n");
            return;
        }
    }
    return;
}

/* Thread that will send signals */
void
sig_thread2 (void *in, void **out)
{
    nk_thread_t *thread1 = (nk_thread_t*)in;
 
    /* wait for custom handler to be registered w/ do_sigaction() */
    nk_counting_barrier(&barrier);

    /* Send first signal to t1, should use "Hello World" signal handler */
    nk_vc_printf("Sending signal to thread: %p.\n", thread1);
    if (nk_signal_send(12, 0, thread1, SIG_DEST_TYPE_THREAD)) {
        nk_vc_printf("Couldn't send signal. Sigtest failed.\n");
        return;
    }

    /* Send first signal to t1, should use custom signal handler */
    nk_vc_printf("Sending signal to thread: %p.\n", thread1);
    if (nk_signal_send(17, 0, thread1, SIG_DEST_TYPE_THREAD)) {
        nk_vc_printf("Couldn't send signal. Sigtest failed.\n");
        return;
    }

    /* Wait for t1 to handle first signal #17 */
    nk_counting_barrier(&barrier);

    /* Should be back to "Hello World" signal handler! */
    nk_vc_printf("Sending signal to thread: %p.\n", thread1);
    if (nk_signal_send(17, 0, thread1, SIG_DEST_TYPE_THREAD)) {
        nk_vc_printf("Couldn't send signal. Sigtest failed.\n");
        return;
    }

    /* Should be custom signal handler, causing thread 1 to exit */
    nk_vc_printf("Sending signal to thread: %p.\n", thread1);
    if (nk_signal_send(18, 0, thread1, SIG_DEST_TYPE_THREAD)) {
        nk_vc_printf("Couldn't send signal. Sigtest failed.\n");
        return;
    }
    nk_vc_printf("Thread 2 exiting. Success?\n");
}


// create two threads, have 1 thread signal the other
static int
handle_sigtest (char * buf, void * priv)
{
    nk_counting_barrier_init(&barrier, 2);
    nk_thread_t *thread1 = 0;
    nk_thread_t *thread2 = 0;

    if (nk_thread_create(sig_thread1, 0, 0, 0, 0, (void **)&thread1, -1)) {
        nk_vc_printf("handle_sigtest: Failed to create new thread\n");
        return -1;
    }
    if (nk_thread_run(thread1)) {
        nk_vc_printf("handle_sigtest: Failed to run thread 1\n");
        return -1;
    }

    if (nk_thread_create(sig_thread2, (void *)thread1, 0, 0, 0, (void **)&thread2, -1)) {
        nk_vc_printf("handle_sigtest: Failed to create new thread\n");
        return -1;
    }
    if (nk_thread_run(thread2)) {
        nk_vc_printf("handle_sigtest: Failed to run thread 2\n");
        return -1;
    }
  
    return 0;
}

static struct shell_cmd_impl signal_test_impl = {
  .cmd      = "sigtest",
  .help_str = "sigtest",
  .handler  = handle_sigtest,
};

nk_register_shell_cmd(signal_test_impl);

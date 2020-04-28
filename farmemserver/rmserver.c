#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <rdma/rdma_cma.h>

#define TEST_NZ(x) do { if ( (x)) die("error: " #x " failed (returned non-zero)." ); } while (0)
#define TEST_Z(x)  do { if (!(x)) die("error: " #x " failed (returned zero/null)."); } while (0)

const size_t BUFFER_SIZE = 1024 * 1024 * 1024 * 32l;
const unsigned int NUM_PROCS = 8;
const unsigned int NUM_QUEUES_PER_PROC = 3;
const unsigned int NUM_QUEUES = NUM_PROCS * NUM_QUEUES_PER_PROC;

struct device {
  struct ibv_pd *pd;
  struct ibv_context *verbs;
};

struct queue {
  struct ibv_qp *qp;
  struct ibv_cq *cq;
  struct rdma_cm_id *cm_id;
  struct ctrl *ctrl;
  enum {
    INIT,
    CONNECTED
  } state;
};

struct ctrl {
  struct queue *queues;
  struct ibv_mr *mr_buffer;
  void *buffer;
  struct device *dev;

  struct ibv_comp_channel *comp_channel;
};

struct memregion {
  uint64_t baseaddr;
  uint32_t key;
};

static void die(const char *reason);

static int alloc_control();
static int on_connect_request(struct rdma_cm_id *id, struct rdma_conn_param *param);
static int on_connection(struct queue *q);
static int on_disconnect(struct queue *q);
static int on_event(struct rdma_cm_event *event);
static void destroy_device(struct ctrl *ctrl);

static struct ctrl *gctrl = NULL;
static unsigned int queue_ctr = 0;

int main(int argc, char **argv)
{
  struct sockaddr_in addr = {};
  struct rdma_cm_event *event = NULL;
  struct rdma_event_channel *ec = NULL;
  struct rdma_cm_id *listener = NULL;
  uint16_t port = 0;

  if (argc != 2) {
    die("Need to specify a port number to listen");
  }

  addr.sin_family = AF_INET;
  addr.sin_port = htons(atoi(argv[1]));

  TEST_NZ(alloc_control());

  TEST_Z(ec = rdma_create_event_channel());
  TEST_NZ(rdma_create_id(ec, &listener, NULL, RDMA_PS_TCP));
  TEST_NZ(rdma_bind_addr(listener, (struct sockaddr *)&addr));
  TEST_NZ(rdma_listen(listener, NUM_QUEUES + 1));
  port = ntohs(rdma_get_src_port(listener));
  printf("listening on port %d.\n", port);

  for (unsigned int i = 0; i < NUM_QUEUES; ++i) {
    printf("waiting for queue connection: %d\n", i);
    struct queue *q = &gctrl->queues[i];

    // handle connection requests
    while (rdma_get_cm_event(ec, &event) == 0) {
      struct rdma_cm_event event_copy;

      memcpy(&event_copy, event, sizeof(*event));
      rdma_ack_cm_event(event);

      if (on_event(&event_copy) || q->state == queue::CONNECTED)
        break;
    }
  }

  printf("done connecting all queues\n");

  // handle disconnects, etc.
  while (rdma_get_cm_event(ec, &event) == 0) {
    struct rdma_cm_event event_copy;

    memcpy(&event_copy, event, sizeof(*event));
    rdma_ack_cm_event(event);

    if (on_event(&event_copy))
      break;
  }

  rdma_destroy_event_channel(ec);
  rdma_destroy_id(listener);
  destroy_device(gctrl);
  return 0;
}

void die(const char *reason)
{
  fprintf(stderr, "%s - errno: %d\n", reason, errno);
  exit(EXIT_FAILURE);
}

int alloc_control()
{
  gctrl = (struct ctrl *) malloc(sizeof(struct ctrl));
  TEST_Z(gctrl);
  memset(gctrl, 0, sizeof(struct ctrl));

  gctrl->queues = (struct queue *) malloc(sizeof(struct queue) * NUM_QUEUES);
  TEST_Z(gctrl->queues);
  memset(gctrl->queues, 0, sizeof(struct queue) * NUM_QUEUES);
  for (unsigned int i = 0; i < NUM_QUEUES; ++i) {
    gctrl->queues[i].ctrl = gctrl;
    gctrl->queues[i].state = queue::INIT;
  }


  return 0;
}

static device *get_device(struct queue *q)
{
  struct device *dev = NULL;

  if (!q->ctrl->dev) {
    dev = (struct device *) malloc(sizeof(*dev));
    TEST_Z(dev);
    dev->verbs = q->cm_id->verbs;
    TEST_Z(dev->verbs);
    dev->pd = ibv_alloc_pd(dev->verbs);
    TEST_Z(dev->pd);

    struct ctrl *ctrl = q->ctrl;
    ctrl->buffer = malloc(BUFFER_SIZE);
    TEST_Z(ctrl->buffer);

    TEST_Z(ctrl->mr_buffer = ibv_reg_mr(
      dev->pd,
      ctrl->buffer,
      BUFFER_SIZE,
      IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ));

    printf("registered memory region of %zu bytes\n", BUFFER_SIZE);
    q->ctrl->dev = dev;
  }

  return q->ctrl->dev;
}

static void destroy_device(struct ctrl *ctrl)
{
  TEST_Z(ctrl->dev);

  ibv_dereg_mr(ctrl->mr_buffer);
  free(ctrl->buffer);
  ibv_dealloc_pd(ctrl->dev->pd);
  free(ctrl->dev);
  ctrl->dev = NULL;
}

static void create_qp(struct queue *q)
{
  struct ibv_qp_init_attr qp_attr = {};

  qp_attr.send_cq = q->cq;
  qp_attr.recv_cq = q->cq;
  qp_attr.qp_type = IBV_QPT_RC;
  qp_attr.cap.max_send_wr = 10;
  qp_attr.cap.max_recv_wr = 10;
  qp_attr.cap.max_send_sge = 1;
  qp_attr.cap.max_recv_sge = 1;

  TEST_NZ(rdma_create_qp(q->cm_id, q->ctrl->dev->pd, &qp_attr));
  q->qp = q->cm_id->qp;
}

int on_connect_request(struct rdma_cm_id *id, struct rdma_conn_param *param)
{

  struct rdma_conn_param cm_params = {};
  struct ibv_device_attr attrs = {};
  struct queue *q = &gctrl->queues[queue_ctr++];

  TEST_Z(q->state == queue::INIT);
  printf("%s\n", __FUNCTION__);

  id->context = q;
  q->cm_id = id;

  struct device *dev = get_device(q);
  create_qp(q);

  TEST_NZ(ibv_query_device(dev->verbs, &attrs));

  printf("attrs: max_qp=%d, max_qp_wr=%d, max_cq=%d max_cqe=%d \
          max_qp_rd_atom=%d, max_qp_init_rd_atom=%d\n", attrs.max_qp,
          attrs.max_qp_wr, attrs.max_cq, attrs.max_cqe,
          attrs.max_qp_rd_atom, attrs.max_qp_init_rd_atom);

  printf("ctrl attrs: initiator_depth=%d responder_resources=%d\n",
      param->initiator_depth, param->responder_resources);

  // the following should hold for initiator_depth:
  // initiator_depth <= max_qp_init_rd_atom, and
  // initiator_depth <= param->initiator_depth
  cm_params.initiator_depth = param->initiator_depth;
  // the following should hold for responder_resources:
  // responder_resources <= max_qp_rd_atom, and
  // responder_resources >= param->responder_resources
  cm_params.responder_resources = param->responder_resources;
  cm_params.rnr_retry_count = param->rnr_retry_count;
  cm_params.flow_control = param->flow_control;

  TEST_NZ(rdma_accept(q->cm_id, &cm_params));

  return 0;
}

int on_connection(struct queue *q)
{
  printf("%s\n", __FUNCTION__);
  struct ctrl *ctrl = q->ctrl;

  TEST_Z(q->state == queue::INIT);

  if (q == &ctrl->queues[0]) {
    struct ibv_send_wr wr = {};
    struct ibv_send_wr *bad_wr = NULL;
    struct ibv_sge sge = {};
    struct memregion servermr = {};

    printf("connected. sending memory region info.\n");
    printf("MR key=%u base vaddr=%p\n", ctrl->mr_buffer->rkey, ctrl->mr_buffer->addr);

    servermr.baseaddr = (uint64_t) ctrl->mr_buffer->addr;
    servermr.key  = ctrl->mr_buffer->rkey;

    wr.opcode = IBV_WR_SEND;
    wr.sg_list = &sge;
    wr.num_sge = 1;
    wr.send_flags = IBV_SEND_SIGNALED | IBV_SEND_INLINE;

    sge.addr = (uint64_t) &servermr;
    sge.length = sizeof(servermr);

    TEST_NZ(ibv_post_send(q->qp, &wr, &bad_wr));

    // TODO: poll here
  }

  q->state = queue::CONNECTED;
  return 0;
}

int on_disconnect(struct queue *q)
{
  printf("%s\n", __FUNCTION__);

  if (q->state == queue::CONNECTED) {
    q->state = queue::INIT;
    rdma_destroy_qp(q->cm_id);
    rdma_destroy_id(q->cm_id);
  }

  return 0;
}

int on_event(struct rdma_cm_event *event)
{
  printf("%s\n", __FUNCTION__);
  struct queue *q = (struct queue *) event->id->context;

  switch (event->event) {
    case RDMA_CM_EVENT_CONNECT_REQUEST:
      return on_connect_request(event->id, &event->param.conn);
    case RDMA_CM_EVENT_ESTABLISHED:
      return on_connection(q);
    case RDMA_CM_EVENT_DISCONNECTED:
      on_disconnect(q);
      return 1;
    default:
      printf("unknown event: %s\n", rdma_event_str(event->event));
      return 1;
  }
}


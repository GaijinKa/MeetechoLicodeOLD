/*
 * NiceConnection.cpp
 */

#include <glib.h>
#include <nice/nice.h>

#include "NiceConnection.h"
#include "SdpInfo.h"

namespace erizo {

guint stream_id;
GSList* lcands;
int streamsGathered;
int rec, sen;
int length;

void cb_nice_recv(NiceAgent* agent, guint stream_id, guint component_id,
		guint len, gchar* buf, gpointer user_data) {

//	printf( "cb_nice_recv len %u id %u\n",len, stream_id );
	NiceConnection* nicecon = (NiceConnection*) user_data;
	nicecon->getWebRtcConnection()->receiveNiceData(reinterpret_cast<char*> (buf), static_cast<unsigned int> (len),
			(NiceConnection*) user_data);
}

void cb_candidate_gathering_done(NiceAgent *agent, guint stream_id,
		gpointer user_data) {

	NiceConnection *conn = (NiceConnection*) user_data;
	//printf("ConnState %u\n",conn->state);
	// ... Wait until the signal candidate-gathering-done is fired ...
	int currentCompId = 1;
	lcands = nice_agent_get_local_candidates(agent, stream_id, currentCompId++);
	NiceCandidate *cand;
	GSList* iterator;
        gchar *ufrag = NULL, *upass = NULL;
        nice_agent_get_local_credentials(agent, stream_id, &ufrag, &upass);
	printf("UPASS %s\n",upass);
        printf("UFRAG %s\n",ufrag);

//////False candidate for testing when there is no network (like in the train) :)
 /* 
		NiceCandidate* thecandidate = nice_candidate_new(NICE_CANDIDATE_TYPE_HOST);
		NiceAddress* naddr = nice_address_new();
		nice_address_set_from_string(naddr, "127.0.0.1");
		nice_address_set_port(naddr, 50000);
		thecandidate->addr = *naddr;
		char* uname = (char*) malloc(50);
		char* pass = (char*) malloc(50);
		sprintf(thecandidate->foundation, "%s", "1");
		sprintf(uname, "%s", "Pedro");
		sprintf(pass, "%s", "oooo");

		thecandidate->username = uname;
		thecandidate->password = pass;
		thecandidate->stream_id = (guint) 1;
		thecandidate->component_id = 1;
		thecandidate->iority = 1000;
		thecandidate->transport = NICE_CANDIDATE_TRANSPORT_UDP;
		lcands = g_slist_append(lcands, thecandidate);
*/

	//	printf("gathering done %u\n",stream_id);
	//printf("Candidates---------------------------------------------------->\n");
	while (lcands != NULL) {
		for (iterator = lcands; iterator; iterator = iterator->next) {
			char address[40], baseAddress[40];
			cand = (NiceCandidate*) iterator->data;
			nice_address_to_string(&cand->addr, address);
			nice_address_to_string(&cand->base_addr, baseAddress);
			if (strstr(address, ":") != NULL) {
				printf("Ignoring IPV6 candidate\n");
				continue;

			}
//			printf("foundation %s\n", cand->foundation);
//			printf("compid %u\n", cand->component_id);
//			printf("stream_id %u\n", cand->stream_id);
//			printf("priority %u\n", cand->priority);
//			printf("username %s\n", cand->username);
//			printf("password %s\n", cand->password);
			CandidateInfo cand_info;
			cand_info.componentId = cand->component_id;
			cand_info.foundation = cand->foundation;
			cand_info.priority = cand->priority;
			cand_info.hostAddress = std::string(address);
			cand_info.hostPort = nice_address_get_port(&cand->addr);
			cand_info.mediaType = conn->mediaType;


			printf("Address Candidate %s\n",address);
			printf("Host Port %d\n", cand_info.hostPort);
			printf("Media Type %d\n", cand_info.mediaType);
                        printf("priority %d\n", cand_info.priority);

			/*
			 *   NICE_CANDIDATE_TYPE_HOST,
			 * 	  NICE_CANDIDATE_TYPE_SERVER_REFLEXIVE,
			 *	  NICE_CANDIDATE_TYPE_PEER_REFLEXIVE,
			 * 	  NICE_CANDIDATE_TYPE_RELAYED,
			 */
			switch (cand->type) {
			case NICE_CANDIDATE_TYPE_HOST:
				cand_info.hostType = HOST;
				break;
			case NICE_CANDIDATE_TYPE_SERVER_REFLEXIVE:
				cand_info.hostType = SRLFX;
				cand_info.baseAddress = std::string(baseAddress);
 			        cand_info.basePort = nice_address_get_port(&cand->base_addr);
				break;
			case NICE_CANDIDATE_TYPE_PEER_REFLEXIVE:
				cand_info.hostType = PRFLX;
				break;
			case NICE_CANDIDATE_TYPE_RELAYED:
				printf("WARNING TURN NOT IMPLEMENTED YET\n");
				cand_info.hostType = RELAY;
				break;
			default:
				break;
			}
			cand_info.netProtocol = "udp";
			cand_info.transProtocol = std::string(*conn->transportName);
			//cand_info.username = std::string(cand->username);
        		cand_info.username = std::string(ufrag);

        		cand_info.password = std::string(upass);

/*			if (cand->username)
				cand_info.username = std::string(cand->username);
			else
				cand_info.username = std::string("(null)");

			if (cand->password)
				cand_info.password = std::string(cand->password);
			else
				cand_info.password = std::string("(null)");
*/
			conn->localCandidates->push_back(cand_info);
		}
		lcands = nice_agent_get_local_candidates(agent, stream_id,
				currentCompId++);
	}
	printf("candidate_gathering done with %d candidates\n", conn->localCandidates->size());
  if (conn->localCandidates->size()==0){
    printf("No local candidates found, check your network connection\n");
    exit(0);
  }
	conn->updateIceState(CANDIDATES_GATHERED);
}

void cb_component_state_changed(NiceAgent *agent, guint stream_id,
		guint component_id, guint state, gpointer user_data) {
	printf("cb_component_state_changed %u\n", state);
	if (state == NICE_COMPONENT_STATE_READY) {
		NiceConnection *conn = (NiceConnection*) user_data;
		conn->updateIceState(READY);
	} else if (state == NICE_COMPONENT_STATE_FAILED) {
		printf("Ice Component failed\n");
		NiceConnection *conn = (NiceConnection*) user_data;
		conn->updateIceState(FAILED);
		//conn->getWebRtcConnection()->close();
	}

}

void cb_new_selected_pair(NiceAgent *agent, guint stream_id, guint component_id,
		gchar *lfoundation, gchar *rfoundation, gpointer user_data) {

	NiceConnection *conn = (NiceConnection*) user_data;
	conn->updateComponentState(component_id, READY);
//	printf(
//			"cb_new_selected_pair for stream %u, comp %u, lfound %s, rfound %s \n",
//			stream_id, component_id, lfoundation, rfoundation);
}

NiceConnection::NiceConnection(MediaType med,
		const std::string &transport_name, int iceComponents, const std::string& stunServer,
    int stunPort, int minPort, int maxPort):mediaType(med),iceComponents_(iceComponents), 
    stunServer_(stunServer), stunPort_ (stunPort), minPort_(minPort), maxPort_(maxPort) {
	agent_ = NULL;
	loop_ = NULL;
	conn_ = NULL;
	localCandidates = new std::vector<CandidateInfo>();
	transportName = new std::string(transport_name);
    for (unsigned int i = 1; i<=iceComponents; i++) {
      comp_state_list[i] = INITIAL;
    }
}

NiceConnection::~NiceConnection() {

	if (iceState != FINISHED)
		this->close();
	if (agent_)
		g_object_unref(agent_);
	if (localCandidates)
		delete localCandidates;
	if (transportName)
		delete transportName;
}

void NiceConnection::join() {

	m_Thread_.join();
}

void NiceConnection::start() {

	m_Thread_ = boost::thread(&NiceConnection::init, this);
}

void NiceConnection::close() {

	if (agent_ != NULL)
		nice_agent_remove_stream(agent_, 1);
	if (loop_ != NULL)
		g_main_loop_quit(loop_);
	iceState = FINISHED;
}

int NiceConnection::sendData(void* buf, int len) {

	int val = -1;
	if (iceState == READY) {
		val = nice_agent_send(agent_, 1, 1, len, reinterpret_cast<const gchar*>(buf));
	}
	return val;
}

WebRtcConnection* NiceConnection::getWebRtcConnection() {

	return conn_;
}

void NiceConnection::init() {

	streamsGathered = 0;
	this->updateIceState(INITIAL);

	g_type_init();
//	g_thread_init(NULL);

	context_ = g_main_context_new();

	loop_ = g_main_loop_new(context_, FALSE);
	//	nice_debug_enable( TRUE );
	// Create a nice agent
	agent_ = nice_agent_new(context_,
		 NICE_COMPATIBILITY_RFC5245);


 GValue controllingMode = { 0 };
    g_value_init(&controllingMode, G_TYPE_BOOLEAN);
    g_value_set_boolean(&controllingMode, false);
    g_object_set_property(G_OBJECT( agent_ ), "controlling-mode", &controllingMode);

//	NiceAddress* naddr = nice_address_new();
//	nice_agent_add_local_address(agent_, naddr);

  if (!stunServer_.compare("") && stunPort_!=0){
    GValue val = { 0 }, val2 = { 0 };
    g_value_init(&val, G_TYPE_STRING);
    g_value_set_string(&val, stunServer_.c_str());
    g_object_set_property(G_OBJECT( agent_ ), "stun-server", &val);

    g_value_init(&val2, G_TYPE_UINT);
    g_value_set_uint(&val2, stunPort_);
	  g_object_set_property(G_OBJECT( agent_ ), "stun-server-port", &val2);
  }

	// Connect the signals
	g_signal_connect( G_OBJECT( agent_ ), "candidate-gathering-done",
			G_CALLBACK( cb_candidate_gathering_done ), this);
	g_signal_connect( G_OBJECT( agent_ ), "component-state-changed",
			G_CALLBACK( cb_component_state_changed ), this);
	g_signal_connect( G_OBJECT( agent_ ), "new-selected-pair",
			G_CALLBACK( cb_new_selected_pair ), this);

	// Create a new stream and start gathering candidates
  printf("Adding Stream... Number of components %d\n", iceComponents_);
	int res = nice_agent_add_stream(agent_, iceComponents_);
	// Set Port Range ----> If this doesn't work when linking the file libnice.sym has to be modified to include this call
  if (minPort_!=0 && maxPort_!=0){
  	nice_agent_set_port_range(agent_, (guint)1, (guint)1, (guint)minPort_, (guint)maxPort_);
  }

	nice_agent_gather_candidates(agent_, 1);
	nice_agent_attach_recv(agent_, 1, 1, g_main_loop_get_context(loop_),
			cb_nice_recv, this);


 if (iceComponents_ > 1) {
      nice_agent_attach_recv(agent_, 1, 2, context_,
          cb_nice_recv, this);
    }
	// Attach to the component to receive the data
	g_main_loop_run(loop_);
}

bool NiceConnection::setRemoteCandidates(
		std::vector<CandidateInfo> &candidates) {


    for (unsigned int compId = 1; compId <= iceComponents_; compId++) {

	GSList* candList = NULL;

	for (unsigned int it = 0; it < candidates.size(); it++) {
		NiceCandidateType nice_cand_type;
		CandidateInfo cinfo = candidates[it];
		if (cinfo.mediaType != this->mediaType	|| cinfo.componentId != compId)
			continue;

		switch (cinfo.hostType) {
		case HOST:
			nice_cand_type = NICE_CANDIDATE_TYPE_HOST;
			break;
		case SRLFX:
			nice_cand_type = NICE_CANDIDATE_TYPE_SERVER_REFLEXIVE;
			break;
		case PRFLX:
			nice_cand_type = NICE_CANDIDATE_TYPE_PEER_REFLEXIVE;
			break;
		case RELAY:
			nice_cand_type = NICE_CANDIDATE_TYPE_RELAYED;
			break;
		default:
			nice_cand_type = NICE_CANDIDATE_TYPE_HOST;
			break;
		}

		NiceCandidate* thecandidate = nice_candidate_new(nice_cand_type);
		NiceAddress* naddr = nice_address_new();
		nice_address_set_from_string(naddr, cinfo.hostAddress.c_str());
		nice_address_set_port(naddr, cinfo.hostPort);
		thecandidate->addr = *naddr;
		sprintf(thecandidate->foundation, "%s", cinfo.foundation.c_str());

		thecandidate->username = strdup(cinfo.username.c_str());
		thecandidate->password = strdup(cinfo.password.c_str());
		thecandidate->stream_id = (guint) 1;
		thecandidate->component_id = cinfo.componentId;
		thecandidate->priority = cinfo.priority;
		thecandidate->transport = NICE_CANDIDATE_TRANSPORT_UDP;
		candList = g_slist_append(candList, thecandidate);

	}

	nice_agent_set_remote_candidates(agent_, (guint) 1, 1, candList);
     }
	printf("Candidates SET\n");
	this->updateIceState(CANDIDATES_RECEIVED);
	return true;
}

void NiceConnection::setWebRtcConnection(WebRtcConnection* connection) {
	this->conn_ = connection;
}

void NiceConnection::updateIceState(IceState state) {
	this->iceState = state;
	if (this->conn_ != NULL)
		this->conn_->updateState(state, this);
}

void NiceConnection::updateComponentState(unsigned int compId, IceState state) {
    comp_state_list[compId] = state;
    if (state == READY) {
      for (unsigned int i = 1; i<=iceComponents_; i++) {
        if (comp_state_list[i] != READY) {
          return;
        }
      }
    }
    this->updateIceState(state);
  }


} /* namespace erizo */

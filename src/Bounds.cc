#include "Bounds.h"
#include <iostream>

using namespace std;

void Bounds::backUp(Belief& belief)
{
    bool debug = false;

    // backup all the actions first
    // if new belief, check init policies
    BeliefNode& beliefNode = *(belief.beliefNode);

    if (debug) {
        cout<<"Bounds::backUp\n";
    }

    if (beliefNode.lastUpdated == Never)
        initBeliefForBackUp(belief);
    backUpActions(belief);

    // find best bounds at the belief
    beliefNode.lBound = NegInf;
    beliefNode.uBound = NegInf;

    for (long i = model.getNumInitPolicies();
         i < model.getNumInitPolicies() + model.getNumActs(); i++) {

        if (debug) {
            cout<<"Bounds::backUp "<<i<<" => "<<beliefNode.actNodes[i]->avgLower<<"\n";
        }

        if (beliefNode.lBound < beliefNode.actNodes[i]->avgLower){
            beliefNode.lBound = beliefNode.actNodes[i]->avgLower;

            if (debug) {
                cout<<"Bounds::backUp Change bestLBoundAct to "<<i<<"\n";
            }
            beliefNode.bestLBoundAct.setActNum(i);
        }

        if (beliefNode.uBound < beliefNode.actNodes[i]->avgUpper){
            beliefNode.uBound = beliefNode.actNodes[i]->avgUpper;

            if (debug) {
                cout<<"Bounds::backUp Change bestUBoundAct to "<<i<<"\n";
            }
            beliefNode.bestUBoundAct.setActNum(i);
        }
    }

    // construct new policy node and try to insert
    PolicyGraph::Node *tempNode = new PolicyGraph::Node;
    tempNode->action = beliefNode.bestLBoundAct;
    long actIndex = beliefNode.bestLBoundAct.actNum;

    if (debug) {
        cout<<beliefNode.actNodes[actIndex]->avgLower<<"\n";
        cout<<"beliefNode.actNodes[beliefNode.bestLBoundAct.actNum]->obsChildren.size() = "<<(beliefNode.actNodes[actIndex]->obsChildren.size())<<"\n";
    }

    // construct policy node observation edges
    for (map<Obs,ObsEdge>::iterator iter = beliefNode.actNodes[actIndex]->obsChildren.begin();
         iter != beliefNode.actNodes[actIndex]->obsChildren.end();
         ++iter){
        if (debug) {
            cout<<"Bounds::backUp temp"<<"\n";
        }

        PolicyGraph::Edge tempEdge(model.getNumObsVar());
        tempEdge.obs = iter->first;
        tempEdge.nextNode = iter->second.bestPolicyNode;
        tempNode->edges.push_back(tempEdge);
    }

    //insert into graph
    pair<PolicyGraph::Node *, bool> outcome = policyGraph.insert(tempNode, model.getObsGrpFromObs(beliefNode.obs));
    // delete if node already present
    if (!outcome.second){
        delete tempNode;
    }
    beliefNode.bestPolicyNode = outcome.first;
    beliefNode.lastUpdated = policyGraph.getSize(model.getObsGrpFromObs(beliefNode.obs));

    if (debug) {
        cout<<"Leaving backUp\n";
    }
}

// void Bounds::backUpInitPolicies(Belief& belief)
// {
//     bool debug = false;

//     if (debug) {
//         cout<<"Bounds::backUpInitPolicies\n";
//     }

//     BeliefNode& beliefNode = *(belief.beliefNode);

//     for (long i = 0; i < model.getNumInitPolicies(); i++){
//         double policyValue = 0;
//         // run simulation
//         for (long j = 0; j < numRandStreams; j++){
//             RandStream randStream = randSource.getStream(j, 0);
//             Particle currParticle = belief.sample(randStream);
//             State currState = currParticle.state;
//             State nextState(model.getNumStateVar(),0);
//             long currMacroActState = InitMacroActState;
//             long nextMacroActState;
//             double currDiscount = 1;
//             double sumDiscounted = 0;
//             double currReward;
//             for (long k = 0; k <  maxSimulLength; k++){
//                 // Check for terminal state
//                 if (model.isTermState(currState)){
//                     break;
//                 }
//                 currReward = model.initPolicy(currState, i, currMacroActState, nextState, nextMacroActState, randStream);
//                 currMacroActState = nextMacroActState;
//                 sumDiscounted += currDiscount * currReward;
//                 currDiscount *= model.getDiscount();
//                 currState = nextState;
//             }
//             double currWeight = power(model.getDiscount(),currParticle.pathLength);
//             policyValue += currWeight*sumDiscounted;
//         }

//         // compute averages
//         beliefNode.actNodes[i]->avgLower = beliefNode.actNodes[i]->avgUpper = policyValue/numRandStreams;

//         // construct observation edge
//         Obs tempObs(vector<long>(model.getNumObsVar(),0));
//         model.setObsType(tempObs,LoopObs);
//         pair<map<Obs,ObsEdge>::iterator, bool> ret =
//                 beliefNode.actNodes[i]->obsChildren.insert(pair<Obs,ObsEdge >(tempObs,ObsEdge(tempObs, this)));
//         ObsEdge& insertedEdge = ret.first->second;
//         insertedEdge.lower = insertedEdge.upper = policyValue;
//         insertedEdge.bestPolicyNode = policyGraph.getInitPolicy(i);
//     }

//     if (debug) {
//         cout<<"Leaving Bounds::backUpInitPolicies\n";
//     }
// }

void Bounds::buildActNodes(Belief& belief)
{
    BeliefNode& beliefNode = *(belief.beliefNode);
    for (long i = 0; i < model.getNumInitPolicies() + model.getNumActs(); i++) {
        Action* act = new Action(i);
        beliefNode.actNodes.push_back(new ActNode(*act, belief, this));
    }
}

void Bounds::backUpActions(Belief& belief)
{
    bool debug = false;

    if (debug) {
        cout<<"Bounds::backUpActions\n";
    }

    BeliefNode& beliefNode = *(belief.beliefNode);

    //long currMacroActState, nextMacroActState;
    Obs obs(vector<long>(model.getNumObsVar(),0));

    #pragma omp parallel for schedule(guided)
    for (long i = model.getNumInitPolicies();
         i < model.getNumInitPolicies() + model.getNumActs();
         i++){
        if (model.allowableAct(belief, Action(i))){
            // generate partitions of states at observation children of this action
            beliefNode.actNodes[i]->generateObsPartitions();
        }
    }

    for (long i = model.getNumInitPolicies();
         i < model.getNumInitPolicies() + model.getNumActs();
         i++){
        if (model.allowableAct(belief, Action(i))){
            beliefNode.actNodes[i]->backup();
            beliefNode.actNodes[i]->clearObsPartitions();
        }
    }

    if (debug) {
        cout<<"Leaving Bounds::backUpActions\n";
    }
}

void Bounds::initBeliefForBackUp(Belief& belief)
{
    BeliefNode& beliefNode = *(belief.beliefNode);

    buildActNodes(belief);
    beliefNode.lBound = beliefNode.uBound = NegInf;
}

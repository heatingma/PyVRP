#include "Params.h"

#include "XorShift128.h"

#include <cmath>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

Params Params::fromFile(Config const &config, std::string const &instPath)
{
    size_t nbClients = 0;
    size_t vehicleCapacity = INT_MAX;

    // TODO test for k<number> in filename?
    size_t nbVehicles = 0;

    // Manner in which the edge weights are provided. Currently, we support
    // EXPLICIT and FULL_MATRIX, and EUC_2D (in which case we compute them
    // with one decimal precision).
    std::string edgeWeightType, edgeWeightFmt;

    std::vector<std::pair<int, int>> coords;
    std::vector<int> demands;
    std::vector<int> servDurs;
    std::vector<std::pair<int, int>> timeWindows;
    std::vector<std::vector<int>> distMat;
    std::vector<int> releases;

    std::ifstream inputFile(instPath);

    if (!inputFile)
        throw std::invalid_argument("Cannot open " + instPath + ".");

    std::string name, ignore;  // section name and 'ignore' string
    for (inputFile >> name; inputFile && name != "EOF"; inputFile >> name)
    {
        // clang-format off
        if (name.starts_with("NAME")  // ignore these lines
            || name.starts_with("COMMENT")
            || name.starts_with("TYPE"))
        // clang-format on
        {
            std::getline(inputFile, ignore);
            continue;
        }

        if (name.starts_with("DIMENSION"))
        {
            inputFile >> ignore >> nbClients;
            nbClients--;  // minus the depot

            // Resize data to match number of clients with default values.
            coords = {nbClients + 1, {0, 0}};
            demands = std::vector<int>(nbClients + 1, 0);
            servDurs = std::vector<int>(nbClients + 1, 0);
            timeWindows = {nbClients + 1, {0, INT_MAX}};
            releases = std::vector<int>(nbClients + 1, 0);
        }

        else if (name.starts_with("EDGE_WEIGHT_TYPE"))
        {
            inputFile >> ignore >> edgeWeightType;
            if (edgeWeightType != "EXPLICIT" && edgeWeightType != "EUC_2D")
            {
                std::ostringstream msg;
                msg << "Only EDGE_WEIGHT_TYPE = EXPLICIT or EDGE_WEIGHT_TYPE = "
                    << "EUC_2D are understood.";

                throw std::runtime_error(msg.str());
            }
        }

        else if (name.starts_with("EDGE_WEIGHT_FORMAT"))
            inputFile >> ignore >> edgeWeightFmt;

        else if (name.starts_with("CAPACITY"))
            inputFile >> ignore >> vehicleCapacity;

        else if (name.starts_with("VEHICLES"))
            inputFile >> ignore >> nbVehicles;

        // Read the edge weights of an explicit distance matrix
        else if (name.starts_with("EDGE_WEIGHT_SECTION"))
        {
            if (edgeWeightType != "EXPLICIT" || edgeWeightFmt != "FULL_MATRIX")
            {
                std::ostringstream msg;
                msg << "Only EDGE_WEIGHT_FORMAT = FULL_MATRIX is understood "
                    << "when EDGE_WEIGHT_TYPE = EXPLICIT.";

                throw std::runtime_error(msg.str());
            }

            for (size_t i = 0; i <= nbClients; i++)
            {
                distMat.emplace_back(nbClients + 1);
                for (size_t j = 0; j <= nbClients; j++)
                    inputFile >> distMat[i][j];
            }
        }

        else if (name.starts_with("NODE_COORD_SECTION"))
            for (size_t row = 0; row <= nbClients; row++)
            {
                int client, x, y;
                inputFile >> client >> x >> y;
                coords[client - 1] = {x, y};
            }

        // Read the demand of each client (including the depot, which
        // should have demand 0)
        else if (name.starts_with("DEMAND_SECTION"))
        {
            for (size_t row = 0; row <= nbClients; row++)
            {
                int client, demand;
                inputFile >> client >> demand;
                demands[client - 1] = demand;
            }

            if (demands[0] != 0)
                throw std::runtime_error("Nonzero depot demand.");
        }

        else if (name.starts_with("SERVICE_TIME_SECTION"))
        {
            for (size_t row = 0; row <= nbClients; row++)
            {
                int client, serviceDuration;
                inputFile >> client >> serviceDuration;
                servDurs[client - 1] = serviceDuration;
            }

            if (servDurs[0] != 0)
                throw std::runtime_error("Nonzero depot service duration.");
        }

        else if (name.starts_with("RELEASE_TIME_SECTION"))
        {
            for (size_t row = 0; row <= nbClients; row++)
            {
                int client, release;
                inputFile >> client >> release;
                releases[client - 1] = release;
            }

            if (releases[0] != 0)
                throw std::runtime_error("Nonzero depot release time.");
        }

        // Read the time windows of all the clients (the depot should
        // have a time window from 0 to max)
        else if (name.starts_with("TIME_WINDOW_SECTION"))
        {
            for (size_t row = 0; row <= nbClients; row++)
            {
                int client, twEarly, twLate;
                inputFile >> client >> twEarly >> twLate;
                timeWindows[client - 1] = {twEarly, twLate};

                if (twEarly >= twLate)
                {
                    std::ostringstream msg;
                    msg << "Client " << client << ": twEarly (=" << twEarly
                        << ") >= twLate (=" << twLate << ").";

                    throw std::runtime_error(msg.str());
                }
            }

            if (timeWindows[0].first != 0)
                throw std::runtime_error("Nonzero depot twEarly.");
        }

        else if (name.starts_with("DEPOT_SECTION"))
        {
            int idDepot, endOfDepotSection;
            inputFile >> idDepot >> endOfDepotSection;

            if (idDepot != 1)
                throw std::runtime_error("Depot ID is supposed to be 1.");

            if (endOfDepotSection != -1)
                throw std::runtime_error("Expected only one depot.");
        }

        else
            throw std::runtime_error("Section " + name + " not understood.");
    }

    if (edgeWeightType == "EUC_2D")
        for (size_t i = 0; i <= nbClients; i++)
        {
            distMat.emplace_back(nbClients + 1);
            for (size_t j = 0; j <= nbClients; j++)
            {
                auto const xDelta = coords[i].first - coords[j].first;
                auto const yDelta = coords[i].second - coords[j].second;
                auto const dist = std::hypot(xDelta, yDelta);

                // Since this is not necessarily integral, we multiply the
                // resulting number by ten to provide one decimal precision.
                distMat[i][j] = static_cast<int>(10 * dist);
            }
        }

    if (distMat.size() != nbClients + 1)
    {
        auto const msg = "Distance matrix does not match problem size.";
        throw std::runtime_error(msg);
    }

    if (!nbVehicles)
        // Not set, so assume unbounded, that is, we assume there's at least
        // as many trucks as there are clients.
        nbVehicles = nbClients;

    return {config,
            coords,
            demands,
            static_cast<int>(nbVehicles),
            static_cast<int>(vehicleCapacity),
            timeWindows,
            servDurs,
            distMat,
            releases};
}

Params::Params(Config const &config,
               std::vector<std::pair<int, int>> const &coords,
               std::vector<int> const &demands,
               int nbVehicles,
               int vehicleCap,
               std::vector<std::pair<int, int>> const &timeWindows,
               std::vector<int> const &servDurs,
               std::vector<std::vector<int>> const &distMat,
               std::vector<int> const &releases)
    : dist_(distMat),
      maxDist_(dist_.max()),
      config(config),
      nbClients(static_cast<int>(coords.size()) - 1),
      nbVehicles(nbVehicles),
      vehicleCapacity(vehicleCap)
{
    // TODO data checks (partially from Params::fromFile)

    // A reasonable scale for the initial values of the load penalty.
    int const maxDemand = *std::max_element(demands.begin(), demands.end());
    int const initCapPenalty = maxDist_ / std::max(maxDemand, 1);
    penaltyCapacity = std::max(std::min(1000, initCapPenalty), 1);

    penaltyTimeWarp = static_cast<int>(config.initialTimeWarpPenalty);

    clients = std::vector<Client>(nbClients + 1);

    for (size_t idx = 0; idx <= static_cast<size_t>(nbClients); ++idx)
        clients[idx] = {coords[idx].first,
                        coords[idx].second,
                        servDurs[idx],
                        demands[idx],
                        timeWindows[idx].first,
                        timeWindows[idx].second,
                        releases[idx]};

    calculateNeighbours();
}

void Params::calculateNeighbours()
{
    auto proximities
        = std::vector<std::vector<std::pair<int, int>>>(nbClients + 1);

    for (int i = 1; i <= nbClients; i++)  // exclude depot
    {
        auto &proximity = proximities[i];

        for (int j = 1; j <= nbClients; j++)  // exclude depot
        {
            if (i == j)  // exclude the current client
                continue;

            // Compute proximity using Eq. 4 in Vidal 2012. The proximity is
            // computed by the distance, min. wait time and min. time warp
            // going from either i -> j or j -> i, whichever is the least.
            int const maxRelease
                = std::max(clients[i].releaseTime, clients[j].releaseTime);

            // Proximity from j to i
            int const waitTime1 = clients[i].twEarly - dist(j, i)
                                  - clients[j].servDur - clients[j].twLate;
            int const earliestArrival1
                = std::max(maxRelease + dist(0, j), clients[j].twEarly);
            int const timeWarp1 = earliestArrival1 + clients[j].servDur
                                  + dist(j, i) - clients[i].twLate;
            int const prox1 = dist(j, i)
                              + config.weightWaitTime * std::max(0, waitTime1)
                              + config.weightTimeWarp * std::max(0, timeWarp1);

            // Proximity from i to j
            int const waitTime2 = clients[j].twEarly - dist(i, j)
                                  - clients[i].servDur - clients[i].twLate;
            int const earliestArrival2
                = std::max(maxRelease + dist(0, i), clients[i].twEarly);
            int const timeWarp2 = earliestArrival2 + clients[i].servDur
                                  + dist(i, j) - clients[j].twLate;
            int const prox2 = dist(i, j)
                              + config.weightWaitTime * std::max(0, waitTime2)
                              + config.weightTimeWarp * std::max(0, timeWarp2);

            proximity.emplace_back(std::min(prox1, prox2), j);
        }

        std::sort(proximity.begin(), proximity.end());
    }

    neighbours = std::vector<std::vector<int>>(nbClients + 1);

    // First create a set of correlated vertices for each vertex (where the
    // depot is not taken into account)
    std::vector<std::set<int>> set(nbClients + 1);
    size_t const granularity
        = std::min(config.nbGranular, static_cast<size_t>(nbClients) - 1);

    for (int i = 1; i <= nbClients; i++)  // again exclude depot
    {
        auto const &orderProximity = proximities[i];

        for (size_t j = 0; j != granularity; ++j)
            set[i].insert(orderProximity[j].second);
    }

    for (int i = 1; i <= nbClients; i++)
        for (int x : set[i])
            neighbours[i].push_back(x);
}
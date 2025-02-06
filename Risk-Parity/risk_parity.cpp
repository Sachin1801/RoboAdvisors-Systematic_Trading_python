#include <iostream>
#include <fstream> //for file input/output
#include <sstream> // for string parsing using sstream
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
using namespace std;

// A small helper struct to hold rolling statistics for each ETF: sum of returns,
// sum of squares of returns, and the count of data points in the rolling window.
class RollingStats
{
public:
    double sum = 0.0;   // sup up r[i] for the window
    double sumSq = 0.0; // sums up r[i]^2 for the window
    int count = 0;      // Number of data points in the window
};

int main()
{
    // Attempt to open the file
    // reading from file to get date, XLF_ret, XOP_ret, XLK_ret
    ifstream file("./ETF--Data.csv");
    if (!file.is_open())
    {
        // if file cannot be openend then print an error and exit
        cerr << "Error opening file.\n";
        return 1;
    }

    // row 1 is header row
    // row 2 is dummy
    //  so start from row 3
    string line;

    // 2) skip the 1st row(header)
    if (!getline(file, line))
    {
        // if we fail to read even one line, the file is empty or invalid
        cerr << "File is empty or invalid. \n";
        return 1;
    }

    // 3) skip the second row(dummy)
    if (!getline(file, line))
    {
        // if we fail to read even one line, the file is empty or invalid
        cerr << "No actual data after dummy row.\n";
        return 1;
    }

    // prepare to store data
    vector<string> dates;   // will hold all the date strings
    vector<double> XLF_Ret; // will hold all the date strings
    vector<double> XOP_Ret; // will hold all the date strings
    vector<double> XLK_Ret; // will hold all the date strings

    // reading file from row 3
    while (getline(file, line))
    {
        stringstream ss(line);
        string date;
        double rxlf, rxop, rxlk;
        char comma; // used to absord the comma between fields

        // if parsing failed, it could be or malformed line too, so skip if present
        if (!(ss >> date >> comma >> rxlf >> comma >> rxop >> comma >> rxlk))
            continue;

        // if parsing successfull push in respective vectors
        dates.push_back(date);
        XLF_Ret.push_back(rxlf);
        XOP_Ret.push_back(rxop);
        XLK_Ret.push_back(rxlk);
    }
    file.close(); // close the file as we have read everything we need

    const size_t n = dates.size();

    // If we have fewer than 20 data rows, we can't do a 20-day rolling volatility calculation.
    if (n < 20)
    {
        std::cerr << "Not enough data rows.\n";
        return 1;
    }

    // ------------------------------------------------------------------
    // 1. Equal-Weighted Strategy
    // ------------------------------------------------------------------
    // We'll create a vector to hold the daily "equity" (portfolio value) for the equal-weight method.

    vector<double> equityEqual(n, 0.0);
    const double startVal = 100.0;

    // Set day 0's equity to 100.0
    equityEqual[0] = startVal;

    // For each subsequent day t, we compute the average of the three ETF returns
    // and then compound the equity from day t-1 by (1 + dailyRet).
    for (size_t t = 1; t < n; t++)
    {
        double daily_ret = (XLF_Ret[t] + XLK_Ret[t] + XOP_Ret[t]) / 3.0;
        equityEqual[t] = equityEqual[t - 1] * (1 + daily_ret);
    }

    // ------------------------------------------------------------------
    // 2. Vol-Weighted (Risk Parity) Strategy with Rolling 20-day Vol
    //    We'll use a rolling window approach for efficiency.
    // ------------------------------------------------------------------

    const int window_size = 20; // 20 day lookback for volatality

    // We'll store the daily equity for the vol-weighted approach in another vector.
    // Initially filled with zeros.
    std::vector<double> equityVol(n, 0.0);

    // We cannot start this strategy until day 20, because we need a 20-day window of returns
    // to compute the first volatility. So let's initialize day 20's equity to 100.0
    // (that means the earliest day we apply the vol weighting is index 20).
    equityVol[window_size] = startVal;

    // We'll keep track of rolling sums for each ETF:
    // sums of returns, sums of squares of returns, and how many items in the window.

    RollingStats statsXLF, statsXOP, statsXLK;

    // 5) First, initialize these rolling stats with the first 20 days: [0 .. 19].
    for (int i = 0; i < window_size; ++i)
    {
        statsXLF.sum += XLF_Ret[i];
        statsXLF.sumSq += XLF_Ret[i] * XLF_Ret[i];

        statsXOP.sum += XOP_Ret[i];
        statsXOP.sumSq += XOP_Ret[i] * XOP_Ret[i];

        statsXLK.sum += XLK_Ret[i];
        statsXLK.sumSq += XLK_Ret[i] * XLK_Ret[i];
    }

    // We know we have exactly 20 data points in that window, so count = 20.
    statsXLF.count = statsXOP.count = statsXLK.count = window_size;

    // 6) Define a small lambda function to compute population standard deviation
    // from the rolling sums. This is so we don't have to recalculate all 20 data
    // points each time. The formula is stdDev = sqrt(meanSq - mean^2).
    // mean = sum / count
    // meanSq = sumSq / count
    // var = meanSq - mean^2
    // stdev = sqrt(var)
    auto stdev = [](const RollingStats &rs)
    {
        double mean = rs.sum / rs.count;     // average of returns
        double meanSq = rs.sumSq / rs.count; // average of returns^2
        double var = meanSq - (mean * mean); // variance = E[X^2] - (E[X])^2
        if (var < 0.0)
            var = 0.0; // numerical safety guard against tiny negative
        return std::sqrt(var);
    };

    // 7) Now iterate from day 21 up to day n-1, computing vol from day t-1 window
    //    and applying those weights to day t's returns.
    //    We also "roll" the window forward each time by removing the oldest day
    //    in the window and adding the newest day.
    for (size_t t = window_size + 1; t < n; ++t)
    {
        // 1) Compute vol at day t-1 using the current rolling stats.
        double volXLF = stdev(statsXLF);
        double volXOP = stdev(statsXOP);
        double volXLK = stdev(statsXLK);

        // 2) Convert them to inverse vol. If a vol is zero, make inverse vol 0 to avoid / 0.
        double invXLF = (volXLF == 0.0) ? 0.0 : (1.0 / volXLF);
        double invXOP = (volXOP == 0.0) ? 0.0 : (1.0 / volXOP);
        double invXLK = (volXLK == 0.0) ? 0.0 : (1.0 / volXLK);

        // Sum them up and normalize so weights sum to 1.
        double sumInv = invXLF + invXOP + invXLK;
        double wXLF = (sumInv > 0.0) ? (invXLF / sumInv) : 0.0;
        double wXOP = (sumInv > 0.0) ? (invXOP / sumInv) : 0.0;
        double wXLK = (sumInv > 0.0) ? (invXLK / sumInv) : 0.0;

        // 3) Now compute day t's portfolio return using these weights.
        // This is the "volWeightedRet" for day t.
        double dailyRetVol = wXLF * XLF_Ret[t] + wXOP * XOP_Ret[t] + wXLK * XLK_Ret[t];

        // Multiply yesterday's equity by (1 + today's return).
        // So equityVol[t] = equityVol[t-1] * (1 + dailyRetVol).
        equityVol[t] = equityVol[t - 1] * (1.0 + dailyRetVol);

        // 4) Roll the window forward:
        //    Remove day (t - window_size - 1) from stats, and add day (t-1).
        //    That ensures the next iteration will represent days [t-window_size, ..., t-1].

        size_t oldIdx = t - window_size - 1; // The day that's leaving the 20-day window
        if (oldIdx < XLF_Ret.size())
        {
            // Subtract the old returns from XLF sums:
            statsXLF.sum -= XLF_Ret[oldIdx];
            statsXLF.sumSq -= XLF_Ret[oldIdx] * XLF_Ret[oldIdx];
            // Subtract the old returns from XOP sums:
            statsXOP.sum -= XOP_Ret[oldIdx];
            statsXOP.sumSq -= XOP_Ret[oldIdx] * XOP_Ret[oldIdx];
            // Subtract the old returns from XLK sums:
            statsXLK.sum -= XLK_Ret[oldIdx];
            statsXLK.sumSq -= XLK_Ret[oldIdx] * XLK_Ret[oldIdx];
        }

        // Add the new day (t-1) to the rolling stats. This day is entering the window.
        size_t newIdx = t - 1;
        statsXLF.sum += XLF_Ret[newIdx];
        statsXLF.sumSq += XLF_Ret[newIdx] * XLF_Ret[newIdx];

        statsXOP.sum += XOP_Ret[newIdx];
        statsXOP.sumSq += XOP_Ret[newIdx] * XOP_Ret[newIdx];

        statsXLK.sum += XLK_Ret[newIdx];
        statsXLK.sumSq += XLK_Ret[newIdx] * XLK_Ret[newIdx];
    }

    // ------------------------------------------------------------------
    // 3. Output or save results to a CSV file so we can view/plot them
    // ------------------------------------------------------------------
    std::ofstream out("results.csv");              // We'll write the output to "results.csv"
    out << "Date,EquityEqual,EquityVolWeighted\n"; // A header row

    // For each day i, we write:
    // Date, equityEqual[i], equityVol[i].
    for (size_t i = 0; i < n; ++i)
    {
        out << dates[i] << ",";
        out << equityEqual[i] << ",";
        out << equityVol[i] << "\n";
    }
    out.close(); // Done writing

    std::cout << "Done! Results in results.csv.\n";
    return 0;
}

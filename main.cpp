#include <iostream>
#include <string>
#include <vector>
#include <unordered_set>
#include <curl/curl.h>
#include <sstream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <thread> // For std::this_thread::sleep_for

namespace pt = boost::property_tree;

// Structure to store open hours for a day
typedef struct OpeningHours
{
    bool open_now = false;
    std::vector<std::pair<std::string, std::string>> periods; // day's opening periods (open time, close time)
    std::vector<std::string> weekday_text;                    // Formatted opening hours text
} OpeningHours;

typedef struct LatLon
{
    float lat;
    float lon;
    LatLon(float lat, float lon) : lat(lat), lon(lon) {}
} LatLon;

typedef struct restaurant_data
{
    std::string url;
    std::string name;
    std::string cuisine;
    std::vector<std::string> types; // Vector to store all restaurant types
    double rating = 0.0;
    LatLon location;
    std::string address;
    std::string place_id;
    OpeningHours hours;         // Added hours information
    bool is_operational = true; // Whether the place is permanently closed
    std::string current_status; // Text describing current open/closed statusp
} restaurant_data;

std::unordered_set<std::string> restaurantTypes = { // set that provides fast lookup for all of google's keys
    "restuarant", "acai_shop", "afghani_restaurant", "african_restaurant", "american_restaurant",
    "asian_restaurant", "bagel_shop", "bakery", "bar", "bar_and_grill", "barbecue_restaurant",
    "brazilian_restaurant", "breakfast_restaurant", "brunch_restaurant", "buffet_restaurant",
    "cafe", "cafeteria", "candy_store", "cat_cafe", "chinese_restaurant", "chocolate_factory",
    "chocolate_shop", "coffee_shop", "confectionery", "deli", "dessert_restaurant", "dessert_shop",
    "diner", "dog_cafe", "donut_shop", "fast_food_restaurant", "fine_dining_restaurant", "food_court",
    "french_restaurant", "greek_restaurant", "hamburger_restaurant", "ice_cream_shop", "indian_restaurant",
    "indonesian_restaurant", "italian_restaurant", "japanese_restaurant", "juice_shop", "korean_restaurant",
    "lebanese_restaurant", "meal_delivery", "meal_takeaway", "mediterranean_restaurant", "mexican_restaurant",
    "middle_eastern_restaurant", "pizza_restaurant", "pub", "ramen_restaurant", "restaurant", "sandwich_shop",
    "seafood_restaurant", "spanish_restaurant", "steak_house", "sushi_restaurant", "tea_house", "thai_restaurant",
    "turkish_restaurant", "vegan_restaurant", "vegetarian_restaurant", "vietnamese_restaurant", "wine_bar"};
std::unordered_map<std::string, std::vector<restaurant_data>> restaurantInfoMap;
std::unordered_map<std::string, std::vector<restaurant_data>> restaurantRatingMap;

// fetch api key
std::string getAPIKey(const std::string &filename)
{
    std::ifstream file(filename);
    std::string line;

    if (file.is_open())
    {
        while (getline(file, line))
        {
            if (line.find("API_KEY=") == 0)
            {
                return line.substr(8); // Extract key after "API_KEY="
            }
        }
        file.close();
    }
}
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *s)
{
    // use a string instead of c_style; doesnt require allocation when appending data and directly works with boost JSON stringstream parsing
    size_t newLength = size * nmemb;
    try
    {
        s->append((char *)contents, newLength);
        return newLength;
    }
    catch (std::bad_alloc &e)
    {
        return 0;
    }
}

// Function to get current day and time for determining if a restaurant is open
std::string get_current_day_of_week()
{
    std::string days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm = *std::localtime(&now_time);
    return days[local_tm.tm_wday];
}

// formats unix time to return a string for human time
std::string get_current_time_string()
{
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm = *std::localtime(&now_time);

    std::stringstream ss;
    // setfill ensures leading zeros (i.e. 3:04 instead of 3:4)
    // setw ensures digit wide (i.e. hours 0-23 or minutes 0-59)
    ss << std::setfill('0') << std::setw(2) << local_tm.tm_hour << ":"
       << std::setfill('0') << std::setw(2) << local_tm.tm_min;
    return ss.str();
}

// Function to fetch details for a specific place including opening hours
bool fetch_place_details(restaurant_data &restaurant, const std::string &api_key)
{
    if (restaurant.place_id.empty())
    {
        return false;
    }

    // Initialize curl
    CURL *curl = curl_easy_init();
    std::string readBuffer;

    if (curl)
    {
        // Build the URL for Place Details API
        std::string url = "https://maps.googleapis.com/maps/api/place/details/json?"
                          "place_id=" +
                          restaurant.place_id +
                          "&fields=name,url,website,opening_hours,permanently_closed" +
                          "&key=" + api_key;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        // Perform the request
        CURLcode res = curl_easy_perform(curl);

        // Check for errors
        if (res != CURLE_OK)
        {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
            curl_easy_cleanup(curl);
            return false;
        }

        try
        {
            // Parse JSON response using Boost property_tree
            pt::ptree root;
            std::stringstream ss(readBuffer);
            pt::read_json(ss, root);

            // Check if the request was successful
            if (root.get<std::string>("status") == "OK")
            {
                const pt::ptree &result = root.get_child("result");

                // Get website URL if available
                try
                {
                    restaurant.url = result.get<std::string>("website");
                }
                catch (pt::ptree_bad_path &)
                {
                    try
                    {
                        restaurant.url = result.get<std::string>("url");
                    }
                    catch (pt::ptree_bad_path &)
                    {
                        restaurant.url = "Not available";
                    }
                }

                // Check if place is permanently closed
                try
                {
                    restaurant.is_operational = !result.get<bool>("permanently_closed");
                }
                catch (pt::ptree_bad_path &)
                {
                    restaurant.is_operational = true; // Assume operational if not specified
                }

                // Get opening hours
                try
                {
                    const pt::ptree &opening_hours = result.get_child("opening_hours");

                    // Check if open now
                    try
                    {
                        restaurant.hours.open_now = opening_hours.get<bool>("open_now");
                    }
                    catch (pt::ptree_bad_path &)
                    {
                        restaurant.hours.open_now = false;
                    }

                    // Get weekday text (formatted opening hours)
                    try
                    {
                        const pt::ptree &weekday_text = opening_hours.get_child("weekday_text");
                        for (const auto &text_pair : weekday_text)
                        {
                            restaurant.hours.weekday_text.push_back(text_pair.second.get_value<std::string>());
                        }
                    }
                    catch (pt::ptree_bad_path &)
                    {
                        // No weekday text available
                    }

                    // Get detailed period information
                    try
                    {
                        const pt::ptree &periods = opening_hours.get_child("periods");
                        for (const auto &period_pair : periods)
                        {
                            const pt::ptree &period = period_pair.second;

                            std::string open_time = period.get_child("open").get<std::string>("time");

                            std::string close_time;
                            try
                            {
                                close_time = period.get_child("close").get<std::string>("time");
                            }
                            catch (pt::ptree_bad_path &)
                            {
                                close_time = "24:00"; // Open 24 hours
                            }

                            restaurant.hours.periods.push_back(std::make_pair(open_time, close_time));
                        }
                    }
                    catch (pt::ptree_bad_path &)
                    {
                        // No periods available
                    }

                    // Determine current status
                    if (!restaurant.is_operational)
                    {
                        restaurant.current_status = "Permanently closed";
                    }
                    else if (restaurant.hours.open_now)
                    {
                        restaurant.current_status = "Currently open";

                        // Try to find when it closes
                        std::string current_day = get_current_day_of_week();
                        for (const auto &text : restaurant.hours.weekday_text)
                        {
                            if (text.find(current_day) != std::string::npos)
                            {
                                size_t close_pos = text.find("–");
                                if (close_pos != std::string::npos)
                                {
                                    restaurant.current_status += " (Closes " + text.substr(close_pos + 1) + ")";
                                }
                                break;
                            }
                        }
                    }
                    else
                    {
                        restaurant.current_status = "Currently closed";

                        // Try to find when it opens next
                        std::string current_day = get_current_day_of_week();
                        bool found_today = false;

                        for (const auto &text : restaurant.hours.weekday_text)
                        {
                            if (text.find(current_day) != std::string::npos)
                            {
                                found_today = true;
                                if (text.find("Closed") != std::string::npos)
                                {
                                    restaurant.current_status += " (Closed today)";
                                }
                                else
                                {
                                    size_t open_pos = text.find(": ");
                                    if (open_pos != std::string::npos)
                                    {
                                        size_t close_pos = text.find("–", open_pos);
                                        if (close_pos != std::string::npos)
                                        {
                                            restaurant.current_status += " (Opens " + text.substr(open_pos + 2, close_pos - open_pos - 3) + ")";
                                        }
                                    }
                                }
                                break;
                            }
                        }

                        if (!found_today)
                        {
                            restaurant.current_status += " (Hours unknown)";
                        }
                    }
                }
                catch (pt::ptree_bad_path &)
                {
                    restaurant.current_status = "Hours not available";
                }
            }
            else
            {
                std::string status = root.get<std::string>("status");
                std::cerr << "API Error: " << status << std::endl;
                try
                {
                    std::string error_msg = root.get<std::string>("error_message");
                    std::cerr << "Error message: " << error_msg << std::endl;
                }
                catch (pt::ptree_bad_path &)
                {
                    // No error message available
                }
                curl_easy_cleanup(curl);
                return false;
            }
        }
        catch (pt::json_parser_error &e)
        {
            std::cerr << "JSON parse error: " << e.what() << std::endl;
            curl_easy_cleanup(curl);
            return false;
        }
        catch (pt::ptree_error &e)
        {
            std::cerr << "Property tree error: " << e.what() << std::endl;
            curl_easy_cleanup(curl);
            return false;
        }

        // Clean up
        curl_easy_cleanup(curl);
        return true;
    }

    return false;
}

// Function to fetch nearby restaurants and return them as a vector of restaurant_data structs
std::vector<restaurant_data> fetch_nearby_restaurants(const std::string &location,
                                                      int radius, int limit, // -1 for unlimited
                                                      const std::string &api_key)
{
    std::vector<restaurant_data> restaurants;

    // Flag to control pagination
    bool has_more_results = true;
    std::string next_page_token = "";

    // Keep fetching while there are more results and we haven't hit the limit
    while (has_more_results && (limit < 0 || restaurants.size() < limit))
    {
        // Initialize curl for this request
        CURL *curl = curl_easy_init();
        std::string readBuffer;

        if (curl)
        {
            // Type of place to search for
            std::string type = "restaurant";

            // Build the URL - either initial request or pagination request
            std::string url;
            if (next_page_token.empty())
            {
                // Initial request
                url = "https://maps.googleapis.com/maps/api/place/nearbysearch/json?"
                      "location=" +
                      location +
                      "&radius=" + std::to_string(radius) +
                      "&type=" + type +
                      "&key=" + api_key;
            }
            else
            {
                // Pagination request using the next page token
                url = "https://maps.googleapis.com/maps/api/place/nearbysearch/json?"
                      "pagetoken=" +
                      next_page_token +
                      "&key=" + api_key;

                // Add a small delay as required by Google API before using a page token
                // This is necessary as the token might not be immediately usable
                std::cout << "Getting next page of results with token..." << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(2000)); // 2-second delay; mandatory for google places api
            }

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

            // Perform the request
            CURLcode res = curl_easy_perform(curl);

            // Check for errors
            if (res != CURLE_OK)
            {
                std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
                curl_easy_cleanup(curl);
                break; // Exit the pagination loop on error
            }
            else
            {
                try
                {
                    // Parse JSON response using Boost property_tree
                    pt::ptree root;
                    std::stringstream ss(readBuffer);
                    pt::read_json(ss, root);

                    // Check if the request was successful
                    if (root.get<std::string>("status") == "OK")
                    {
                        // Get the restaurants from results array
                        const pt::ptree &results = root.get_child("results");
                        int results_this_page = 0;

                        // Process each restaurant result
                        for (const auto &pair : results)
                        {
                            if (limit > 0 && restaurants.size() >= limit)
                            {
                                // We've hit our limit, stop processing
                                has_more_results = false;
                                break;
                            }

                            const pt::ptree &place = pair.second;

                            // Create a restaurant_data struct for the current restaurant
                            restaurant_data restaurant;

                            // Restaurant name
                            restaurant.name = place.get<std::string>("name");

                            // Rating (if available)
                            try
                            {
                                restaurant.rating = place.get<double>("rating");
                            }
                            catch (pt::ptree_bad_path &)
                            {
                                restaurant.rating = 0.0; // Rating not available
                            }

                            // Address
                            try
                            {
                                restaurant.address = place.get<std::string>("vicinity");
                            }
                            catch (pt::ptree_bad_path &)
                            {
                                restaurant.address = "Address not available";
                            }

                            // Extract latitude and longitude
                            restaurant.location = LatLon(static_cast<float>(place.get_child("geometry").get_child("location").get<double>("lat")), static_cast<float>(place.get_child("geometry").get_child("location").get<double>("lng")));
                            // Store all types in the restaurant.types vector
                            try
                            {
                                const pt::ptree &type_array = place.get_child("types");
                                for (const auto &type_pair : type_array)
                                {
                                    std::string type_str = type_pair.second.get_value<std::string>();
                                    restaurant.types.push_back(type_str);
                                }
                            }
                            catch (pt::ptree_bad_path &)
                            {
                                restaurant.types.push_back("No types available");
                            }

                            // Determine cuisine type based on types (using original logic)
                            restaurant.cuisine = "Not specified";
                            for (const auto &type_str : restaurant.types)
                            {
                                // Look for food-related types
                                if (type_str != "restaurant" && type_str != "food" &&
                                    type_str != "establishment" && type_str != "point_of_interest" &&
                                    type_str.find("_store") == std::string::npos)
                                {
                                    restaurant.cuisine = type_str;

                                    // Replace underscores with spaces and capitalize
                                    std::replace(restaurant.cuisine.begin(), restaurant.cuisine.end(), '_', ' ');
                                    if (!restaurant.cuisine.empty())
                                    {
                                        restaurant.cuisine[0] = std::toupper(restaurant.cuisine[0]);
                                    }
                                    break;
                                }
                            }

                            // Check for open_now information in the nearby search results
                            try
                            {
                                const pt::ptree &hours = place.get_child("opening_hours");
                                restaurant.hours.open_now = hours.get<bool>("open_now");
                                restaurant.current_status = restaurant.hours.open_now ? "Currently open" : "Currently closed";
                            }
                            catch (pt::ptree_bad_path &)
                            {
                                restaurant.current_status = "Hours not available";
                            }

                            // Place ID for getting more details
                            try
                            {
                                restaurant.place_id = place.get<std::string>("place_id");
                            }
                            catch (pt::ptree_bad_path &)
                            {
                                restaurant.place_id = "";
                            }

                            // Website would require a second API call
                            restaurant.url = "Not available";

                            // Add the restaurant to our vector
                            restaurants.push_back(restaurant);
                            results_this_page++;
                        }

                        std::cout << "Retrieved " << results_this_page << " restaurants (total: " << restaurants.size() << ")" << std::endl;

                        // Check for next page token
                        try
                        {
                            next_page_token = root.get<std::string>("next_page_token");
                            // There is a next page token, continue pagination
                            has_more_results = true;
                        }
                        catch (pt::ptree_bad_path &)
                        {
                            // No next page token, we're done
                            has_more_results = false;
                            next_page_token = "";
                        }
                    }
                    else
                    {
                        std::string status = root.get<std::string>("status");
                        std::cerr << "API Error: " << status << std::endl;
                        try
                        {
                            std::string error_msg = root.get<std::string>("error_message");
                            std::cerr << "Error message: " << error_msg << std::endl;
                        }
                        catch (pt::ptree_bad_path &)
                        {
                            // No error message available
                        }
                        has_more_results = false; // Stop on error
                    }
                }
                catch (pt::json_parser_error &e)
                {
                    std::cerr << "JSON parse error: " << e.what() << std::endl;
                    has_more_results = false; // Stop on error
                }
                catch (pt::ptree_error &e)
                {
                    std::cerr << "Property tree error: " << e.what() << std::endl;
                    has_more_results = false; // Stop on error
                }
            }

            // Clean up curl for this request
            curl_easy_cleanup(curl);
        }
        else
        {
            // Failed to initialize curlrestaurant.rating
            has_more_results = false;
        }
    }

    return restaurants;
}

// Example main function showing how to use the fetch_nearby_restaurants function
// NOTE THIS FUNCTION NEEDS TO GET THE INTERSECITON COORDINATES (FROM MOUSE CLICK?) AND add an xy to the latlon to each restaurant; should be handled above tbh
void generateRestaurantMaps(std::string location, std::string apiKey)
{
    // Fetch nearby restaurants
    int radius;
    int limit;
    std::cout << "Enter the radius: " << std::endl;
    std::cin >> radius;
    std::cout << "Enter the limit (60 locations max): " << std::endl;
    s
            std::cin >>
        limit;

    std::vector<restaurant_data> restaurants = fetch_nearby_restaurants(location, radius, limit, apiKey);

    // Get detailed information including opening hours for each restaurant
    for (size_t i = 0; i < restaurants.size(); i++)
    {
        // Fetch additional details including opening hourswe actually need this to finish building the structs; im assuming that the structs are biult because the print_restaurant_info gives us smthin
        fetch_place_details(restaurants[i], apiKey);
        // Print information about the restaurant - DEBUGGING
        // print_restaurant_info(restaurants[i], i);
    }

    // this populates our map so it can be used elsewhere
    for (size_t RestaurantIt = 0; RestaurantIt < restaurants.size(); RestaurantIt++)
    {
        const restaurant_data &restaurant = restaurants[RestaurantIt];

        // Iterate through each type in the restaurant's types vector
        for (const std::string &type : restaurant.types)
        {
            restaurantInfoMap[type].push_back(restaurant);
        }
        if (restaurant.rating <= 5.0 || restaurant.rating >= 4.8)
            restaurantRatingMap["5.0-4.8"].push_back(restaurant);
        else if (restaurant.rating <= 4.79 && restaurant.rating >= 4.4)
            restaurantRatingMap["4.79-4.4"].push_back(restaurant);
        else if (restaurant.rating <= 4.39 && restaurant.rating >= 3.8)
            restaurantRatingMap["4.39-3.8"].push_back(restaurant);
        else if (restaurant.rating <= 3.79 && restaurant.rating >= 3.0)
            restaurantRatingMap["3.79-3.0"].push_back(restaurant);
        else if (restaurant.rating <= 2.9 && restaurant.rating >= 2.5)
            restaurantRatingMap["2.9-2.5"].push_back(restaurant);
        else if (restaurant.rating <= 2.49)
            restaurantRatingMap["<=2.49"].push_back(restaurant);
        else
            restaurantRatingMap["no rating"].push_back(restaurant);
    }
}

int main()
{
    std::string lat;
    std::string lon;
    std::cout << "Enter the latitude: " << std::endl;
    std::cin >> lat;
    std::cout << "Enter the longitude: " << std::endl;
    std::cin >> lon;

    std::string apiKey = getAPIKey(".env"); // Read from .env file
    if (!apiKey.empty())
    {
        // Location coordinates (example: New York City)
        std::string location = lat + "," + lon;
        generateRestaurantMaps(location, apiKey);
    }

    return 0;
}
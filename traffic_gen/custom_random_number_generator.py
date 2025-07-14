""" A library defining a custom random number generator based on a 
user-defined cumulative distribution function (CDF). """

import random
from collections import namedtuple


class CdfDataPoint(
    namedtuple("CdfDataPoint", ["value", "cumulative_prob_perc"])
):
    """
    A data point in a cumulative distribution function (CDF), which includes
    the value and the cumulative probability (as a percentage).
    """


class CustomRandomNumberGenerator:
    """
    A custom random number generator based on a user-defined cumulative distribution function (CDF).
    Provides functionalities to set and validate the CDF, calculate the average value of the CDF,
    generate random numbers according to the CDF, and determine percentiles and corresponding
    values.
    """

    def __init__(self):
        """
        Initialize the CustomRandomNumberGenerator object.

        Args:
            seed (int, optional): Seed value for the random number generator.
            Defaults to None, which uses the system time as the seed.
        """
        self.cdf_intra: list[CdfDataPoint] = []
        self.cdf_inter: list[CdfDataPoint] = []

    def is_valid_cdf(self, input_cdf: list[CdfDataPoint]) -> bool:
        """
        Validates the provided CDF. A valid CDF should start at 0% and end at 100%,
        with both x (value) and y (cumulative probability percentage) values monotonically
        increasing.

        Args:
            input_cdf (list of tuple): The cumulative distribution function (CDF) to validate.

        Returns:
            bool: True if the CDF is valid, False otherwise.
        """
        first_cumulative_prob = input_cdf[0].cumulative_prob_perc
        last_cumulative_prob = input_cdf[-1].cumulative_prob_perc
        if (
            not input_cdf
            or first_cumulative_prob != 0
            or last_cumulative_prob != 100
        ):
            return False

        for i in range(1, len(input_cdf)):
            curr_val, curr_cumulative_prob_perc = input_cdf[i]
            prev_val, prev_cumulative_prob_perc = input_cdf[i - 1]
            if (
                curr_cumulative_prob_perc <= prev_cumulative_prob_perc
                or curr_val <= prev_val
            ):
                return False

        return True

    def set_cdf(self, input_cdf_intra: list[tuple[float, float]], input_cdf_inter: list[tuple[float, float]]) -> bool:
        """
        Sets the CDF for the random number generator if the provided CDF is valid.

        Args:
            input_cdf_intra (list of tuple): The cumulative distribution function (CDF) to set for intra-dc.
            input_cdf_inter (list of tuple): The cumulative distribution function (CDF) to set for inter-dc.

        Returns:
            bool: True if the CDFs are valid and successfully set, False otherwise.
        """

        intra_valid = False
        inter_valid = False

        if self.is_valid_cdf(input_cdf_intra):
            self.cdf_intra = input_cdf_intra
            intra_valid = True
        if self.is_valid_cdf(input_cdf_inter):
            self.cdf_inter = input_cdf_inter
            inter_valid = True
        return (intra_valid and inter_valid)

    def set_cdf_from_file(self, cdf_file_path_intra: str, cdf_file_path_inter: str) -> bool:
        """
        Read the CDF from a file and set it for the random number generator
        if the provided CDF is valid. Expected format of the CDF file:

        <value> 0\n
        <value> <cumulative probability percentage>\n
        ...\n
        <value> 100\n


        Args:
            cdf_file_path_intra (str): The path of the CDF file of intra-dc traffic.
            cdf_file_path_inter (str): The path of the CDF file of inter-dc traffic.

        Returns:
            bool: True if the CDFs are valid and successfully set, False otherwise.
        """
        with open(cdf_file_path_intra, "r", encoding="utf-8") as cdf_file:
            cdf_from_file_intra = [
                CdfDataPoint(float(val), float(cumulative_prob_perc))
                for val, cumulative_prob_perc in (
                    line.strip().split() for line in cdf_file
                )
            ]
        with open(cdf_file_path_inter, "r", encoding="utf-8") as cdf_file:
            cdf_from_file_inter = [
                CdfDataPoint(float(val), float(cumulative_prob_perc))
                for val, cumulative_prob_perc in (
                    line.strip().split() for line in cdf_file
                )
            ]
        return self.set_cdf(cdf_from_file_intra, cdf_from_file_inter)

    def calculate_average_value(self, intra_dc_percentage) -> float:
        """
        Calculates and returns the average value of the CDF.

        Returns:
            float: The average value of the CDF.
        """
        total_average_intra = 0
        prev_value, prev_cumulative_prob = self.cdf_intra[0]
        for curr_value, curr_cumulative_prob in self.cdf_intra[1:]:
            total_average_intra += (
                (curr_value + prev_value)
                / 2.0
                * (curr_cumulative_prob - prev_cumulative_prob)
            )
            prev_value, prev_cumulative_prob = (
                curr_value,
                curr_cumulative_prob,
            )
        total_average_intra /= 100.0
        print(f'average msg size intra-DC (B): {total_average_intra}')
        
        total_average_inter = 0
        prev_value, prev_cumulative_prob = self.cdf_inter[0]
        for curr_value, curr_cumulative_prob in self.cdf_inter[1:]:
            total_average_inter += (
                (curr_value + prev_value)
                / 2.0
                * (curr_cumulative_prob - prev_cumulative_prob)
            )
            prev_value, prev_cumulative_prob = (
                curr_value,
                curr_cumulative_prob,
            )
        total_average_inter /= 100.0
        print(f'average msg size inter-DC (B): {total_average_inter}')

        total_average = intra_dc_percentage * total_average_intra + (1.0 - intra_dc_percentage) * total_average_inter

        return total_average

    def generate_random_number(self, is_intra) -> float:
        """
        Generates a random number based on the CDF.

        Args:
            is_intra (bool): indicates whether flow is intra- or inter-dc

        Returns:
            float: A random number generated according to the CDF.
        """
        random_percentage = random.random() * 100
        if (is_intra):
            return self.get_value_from_percentile_intra(random_percentage)
        else:
            return self.get_value_from_percentile_inter(random_percentage)

    def get_value_from_percentile_intra(self, percentile: float) -> float:
        """
        Determines the value corresponding to a given percentile based on the CDF.
        Uses linear interpolation to determine the value.

        Args:
            percentile (float): The percentile to find the value for.

        Returns:
            float: The value corresponding to the given percentile, or None if out of range.
        """
        for i in range(1, len(self.cdf_intra)):
            if percentile <= self.cdf_intra[i].cumulative_prob_perc:
                lower_value, lower_percentile = self.cdf_intra[i - 1]
                upper_value, upper_percentile = self.cdf_intra[i]
                return lower_value + (percentile - lower_percentile) * (
                    (upper_value - lower_value)
                    / (upper_percentile - lower_percentile)
                )

    def get_value_from_percentile_inter(self, percentile: float) -> float:
        """
        Determines the value corresponding to a given percentile based on the CDF.
        Uses linear interpolation to determine the value.

        Args:
            percentile (float): The percentile to find the value for.

        Returns:
            float: The value corresponding to the given percentile, or None if out of range.
        """
        for i in range(1, len(self.cdf_inter)):
            if percentile <= self.cdf_inter[i].cumulative_prob_perc:
                lower_value, lower_percentile = self.cdf_inter[i - 1]
                upper_value, upper_percentile = self.cdf_inter[i]
                return lower_value + (percentile - lower_percentile) * (
                    (upper_value - lower_value)
                    / (upper_percentile - lower_percentile)
                )

    # def get_percentile_from_value(self, value: float) -> float:
    #     """
    #     Determines the percentile of a given value based on the CDF.

    #     Args:
    #         value (float): The value to find the percentile for.

    #     Returns:
    #         float: The percentile of the given value based on the CDF, or -1 if out of range.
    #     """
    #     if value < 0 or value > self.cdf[-1].value:
    #         return -1

    #     for i in range(1, len(self.cdf)):
    #         if value <= self.cdf[i].value:
    #             lower_value, lower_percentile = self.cdf[i - 1]
    #             upper_value, upper_percentile = self.cdf[i]
    #             return lower_percentile + (
    #                 (upper_percentile - lower_percentile)
    #                 / (upper_value - lower_value)
    #                 * (value - lower_value)
    #             )
